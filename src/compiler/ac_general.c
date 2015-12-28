#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast_compiler.h"

extern hashtable global_args;

void die(int lineno, const char *fmt, ...)
{
    size_t len = strlen(fmt);
    char format[len + 9];
    sprintf(format, "Error: %s\n", fmt);
    va_list args;
    va_start(args, fmt); vfprintf(stderr, format, args); fprintf(stderr, "\t-> Line %d\n", lineno);
    va_end(args);

    exit(0);
}

LLVMModuleRef ac_compile(AST *ast)
{
    CompilerBundle cb;
    cb.module = LLVMModuleCreateWithNameInContext("main-module", LLVMGetGlobalContext());
    cb.builder = LLVMCreateBuilder();
    cb.transients = hst_create();
    cb.loadedTransients = hst_create();

    cb.currentLoopEntry = cb.currentLoopExit = NULL;

    cb.td = LLVMCreateTargetData("");
    
    VarScopeStack vs = vs_make();
    cb.varScope = &vs;

    vs_push(cb.varScope);

    if(!hst_get(&global_args, ARGS_COMPILE_RC, NULL, NULL))
        ac_prepare_module(cb.module);
    LLVMTypeRef param_types[] = {LLVMPointerType(LLVMInt8Type(), 0)};
    LLVMTypeRef func_type = LLVMFunctionType(LLVMInt32Type(), param_types, 1, 1);
    LLVMAddFunction(cb.module, "printf", func_type);

    the_module = cb.module;
    /*
    LLVMTypeRef st = LLVMStructCreateNamed(LLVMGetGlobalContext(), "teststruct");
    LLVMTypeRef ty = LLVMInt64Type();
    LLVMStructSetBody(st, &ty, 1, 0);
    */

    AST *old = ast;
    for(; ast; ast = ast->next)
        ac_add_early_name_declaration(ast, &cb);
    ast = old;
    for(; ast; ast = ast->next)
        ac_add_early_declarations(ast, &cb);

    vs_put(cb.varScope, (char *)"__egl_millis", LLVMGetNamedFunction(cb.module, "__egl_millis"), ett_function_type(ett_base_type(ETInt64), NULL, 0));

    ast = old;

    ac_make_struct_definitions(ast, &cb);
    ac_generate_interface_definitions(ast, &cb);
    ac_make_class_definitions(ast, &cb);

    for(; ast; ast = ast->next)
    {
        ac_dispatch_declaration(ast, &cb);
    }

    vs_pop(cb.varScope);

    LLVMDisposeBuilder(cb.builder);
    LLVMDisposeTargetData(cb.td);
    vs_free(cb.varScope);

    hst_free(&cb.transients);
    hst_free(&cb.loadedTransients);

    return cb.module;
}

void ac_prepare_module(LLVMModuleRef module)
{
    LLVMTypeRef param_types_rc[] = {LLVMPointerType(LLVMInt64Type(), 0)};
    LLVMTypeRef func_type_rc = LLVMFunctionType(LLVMVoidType(), param_types_rc, 1, 0);
    LLVMAddFunction(module, "__egl_incr_ptr", func_type_rc);
    LLVMAddFunction(module, "__egl_decr_ptr", func_type_rc);
    LLVMAddFunction(module, "__egl_decr_no_free", func_type_rc);
    LLVMAddFunction(module, "__egl_check_ptr", func_type_rc);
    LLVMAddFunction(module, "__egl_prepare", func_type_rc);

    LLVMTypeRef param_types_we[] = { LLVMPointerType(LLVMInt64Type(), 0), LLVMPointerType(LLVMInt8Type(), 0)};
    func_type_rc = LLVMFunctionType(LLVMVoidType(), param_types_we, 2, 0);
    LLVMAddFunction(module, "__egl_add_weak", func_type_rc);

    LLVMTypeRef ty = LLVMPointerType(LLVMInt8Type(), 0);
    func_type_rc = LLVMFunctionType(LLVMVoidType(), &ty, 1, 0);
    LLVMAddFunction(module, "__egl_remove_weak", func_type_rc);

    LLVMTypeRef param_types_arr1[] = {LLVMPointerType(LLVMInt8Type(), 0), LLVMInt64Type()};
    func_type_rc = LLVMFunctionType(LLVMVoidType(), param_types_arr1, 2, 0);
    LLVMAddFunction(module, "__egl_array_fill_nil", func_type_rc);

    param_types_arr1[0] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
    func_type_rc = LLVMFunctionType(LLVMVoidType(), param_types_arr1, 2, 0);
    LLVMAddFunction(module, "__egl_array_decr_ptrs", func_type_rc);

    func_type_rc = LLVMFunctionType(LLVMInt64Type(), NULL, 0, 0);
    LLVMAddFunction(module, "__egl_millis", func_type_rc);


    LLVMTypeRef param_types_destruct[2];
    param_types_destruct[0] = LLVMPointerType(LLVMInt8Type(), 0);
    param_types_destruct[1] = LLVMInt1Type();
    func_type_rc = LLVMFunctionType(LLVMVoidType(), param_types_destruct, 2, 0);
    LLVMAddFunction(module, "__egl_counted_destructor", func_type_rc);

    LLVMTypeRef lookup_types[] = {LLVMPointerType(LLVMInt8Type(), 0), LLVMPointerType(LLVMInt8Type(), 0), LLVMInt32Type()};
    func_type_rc = LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), lookup_types, 3, 0);
    LLVMAddFunction(module, "__egl_lookup_method", func_type_rc);
}

void ac_add_early_name_declaration(AST *ast, CompilerBundle *cb)
{
    if(ast->type == ASTRUCTDECL)
    {
        ac_add_struct_declaration(ast, cb);
        return;
    }

    if(ast->type == ACLASSDECL)
    {
        ac_add_class_declaration(ast, cb);
        return;
    }
}

void ac_add_early_declarations(AST *ast, CompilerBundle *cb)
{
    if(ast->type == AGENDECL)
    {
        ac_add_gen_declaration(ast, cb);
        return;
    }

    if(ast->type != AFUNCDECL)
        return;

    ASTFuncDecl *a = (ASTFuncDecl *)ast;

    int i, ct;
    AST *p;
    for(p = a->params, i = 0; p; p = p->next, i++);
    ct = i;

    LLVMTypeRef param_types[ct];
    EagleTypeType *eparam_types[ct];
    for(i = 0, p = a->params; p; p = p->next, i++)
    {
        ASTTypeDecl *type = (ASTTypeDecl *)((ASTVarDecl *)p)->atype;

        param_types[i] = ett_llvm_type(type->etype);
        eparam_types[i] = type->etype;

        if(type->etype->type == ETStruct)
            die(ALN, "Passing struct by value not supported.");
    }

    ASTTypeDecl *retType = (ASTTypeDecl *)a->retType;
    if(strcmp(a->ident, "printf") == 0)
    {
        LLVMValueRef func = LLVMGetNamedFunction(cb->module, "printf");
        vs_put(cb->varScope, a->ident, func, ett_function_type(retType->etype, eparam_types, ct));
        return;
    }
    if(strcmp(a->ident, "free") == 0)
    {
        LLVMValueRef func = LLVMGetNamedFunction(cb->module, "free");
        if(func)
        {
            vs_put(cb->varScope, a->ident, func, ett_function_type(retType->etype, eparam_types, ct));
            return;
        }
    }

    LLVMTypeRef func_type = LLVMFunctionType(ett_llvm_type(retType->etype), param_types, ct, a->vararg);
    LLVMValueRef func = LLVMAddFunction(cb->module, a->ident, func_type);

    if(retType->etype->type == ETStruct)
        die(ALN, "Returning struct by value not supported.");

    vs_put(cb->varScope, a->ident, func, ett_function_type(retType->etype, eparam_types, ct));
}

LLVMValueRef ac_dispatch_expression(AST *ast, CompilerBundle *cb)
{
    LLVMValueRef val = NULL;
    switch(ast->type)
    {
        case AVALUE:
            val = ac_compile_value(ast, cb);
            break;
        case ABINARY:
            val = ac_compile_binary(ast, cb);
            break;
        case AUNARY:
            val = ac_compile_unary(ast, cb);
            break;
        case AVARDECL:
            val = ac_compile_var_decl(ast, cb);
            break;
        case AIDENT:
            val = ac_compile_identifier(ast, cb);
            break;
        case AFUNCCALL:
            val = ac_compile_function_call(ast, cb);
            break;
        case ACAST:
            val = ac_compile_cast(ast, cb);
            break;
        case AALLOC:
            val = ac_compile_new_decl(ast, cb);
            break;
        case ASTRUCTMEMBER:
            val = ac_compile_struct_member(ast, cb, 0);
            break;
        case AFUNCDECL:
            val = ac_compile_closure(ast, cb);
            break;
        default:
            die(ALN, "Invalid expression type.");
            return NULL;
    }

    if(!ast->resultantType)
        die(ALN, "Internal Error. AST Resultant Type for expression not set.");
    return val;
}

void ac_dispatch_statement(AST *ast, CompilerBundle *cb)
{
    switch(ast->type)
    {
        case AVALUE:
            ac_compile_value(ast, cb);
            break;
        case ABINARY:
            ac_compile_binary(ast, cb);
            break;
        case AUNARY:
            ac_compile_unary(ast, cb);
            break;
        case AVARDECL:
            ac_compile_var_decl(ast, cb);
            break;
        case AIDENT:
            ac_compile_identifier(ast, cb);
            break;
        case AFUNCCALL:
            ac_compile_function_call(ast, cb);
            break;
        case AIF:
            ac_compile_if(ast, cb, NULL);
            break;
        case ALOOP:
            ac_compile_loop(ast, cb);
            break;
        case ACAST:
            ac_compile_cast(ast, cb);
            break;
        case AALLOC:
            ac_compile_new_decl(ast, cb);
            break;
        case AFUNCDECL:
            ac_compile_closure(ast, cb);
            break;
        default:
            die(ALN, "Invalid statement type.");
    }

    hst_for_each(&cb->transients, ac_decr_transients, cb);
    hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);
    
    hst_free(&cb->transients);
    hst_free(&cb->loadedTransients);

    cb->transients = hst_create();
    cb->loadedTransients = hst_create();
}

void ac_dispatch_declaration(AST *ast, CompilerBundle *cb)
{
    switch(ast->type)
    {
        case AFUNCDECL:
            ac_compile_function(ast, cb);
            break;
        case ASTRUCTDECL:
        case ACLASSDECL:
        case AIFCDECL:
            break;
        case AGENDECL:
            ac_compile_generator_code(ast, cb);
            break;
        default:
            die(ALN, "Invalid declaration type.");
            return;
    }
}
