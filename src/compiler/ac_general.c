/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast_compiler.h"
#include "core/utils.h"
#include "core/colors.h"

extern Hashtable global_args;
extern char *current_file_name;

static void ac_deferment_callback(AST *ast, void *data);

#ifdef RELEASE
void die(int lineno, const char *fmt, ...)
{
    size_t len = strlen(fmt);
    char format[len + 15];
    sprintf(format, LIGHT_RED "Error:" DEFAULT " %s\n", fmt);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, format, args);

    if(lineno < 0)
        fprintf(stderr, LIGHT_BLUE "\tin" DEFAULT " %s\n", current_file_name);
    else
        fprintf(stderr, LIGHT_BLUE "\tat" DEFAULT " %s" LIGHT_BLUE ":" DEFAULT "%d\n", current_file_name, lineno);

    va_end(args);

    exit(0);
}

void warn(int lineno, const char *fmt, ...)
{
    size_t len = strlen(fmt);
    char format[len + 30];
    sprintf(format, LIGHT_YELLOW "Warning:" DEFAULT " %s\n", fmt);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, format, args);

    if(lineno < 0)
        fprintf(stderr, LIGHT_BLUE "\tin" DEFAULT " %s\n", current_file_name);
    else
        fprintf(stderr, LIGHT_BLUE "\tat" DEFAULT " %s" LIGHT_BLUE ":" DEFAULT "%d\n", current_file_name, lineno);

    va_end(args);
}
#else
void die_debug(int complineno, const char *file, int lineno, const char *fmt, ...)
{
    size_t len = strlen(fmt);
    char format[len + 15];
    sprintf(format, LIGHT_RED "Error:" DEFAULT " %s\n", fmt);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, format, args);

    if(lineno < 0)
        fprintf(stderr, LIGHT_BLUE "\tin" DEFAULT " %s\n", current_file_name);
    else
        fprintf(stderr, LIGHT_BLUE "\tat" DEFAULT " %s" LIGHT_BLUE ":" DEFAULT "%d\n", current_file_name, lineno);

    fprintf(stderr, LIGHT_BLUE "\tCompiler:" DEFAULT " %s" LIGHT_BLUE ":" DEFAULT "%d\n", file, complineno);
    va_end(args);

    int *i = NULL;
    exit(*i);
}

void warn_debug(int complineno, const char *file, int lineno, const char *fmt, ...)
{
    size_t len = strlen(fmt);
    char format[len + 30];
    sprintf(format, LIGHT_YELLOW "Warning:" DEFAULT " %s\n", fmt);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, format, args);

    if(lineno < 0)
        fprintf(stderr, LIGHT_BLUE "\tin" DEFAULT " %s\n", current_file_name);
    else
        fprintf(stderr, LIGHT_BLUE "\tat" DEFAULT " %s" LIGHT_BLUE ":" DEFAULT "%d\n", current_file_name, lineno);

    fprintf(stderr, LIGHT_BLUE "\tCompiler:" DEFAULT " %s" LIGHT_BLUE ":" DEFAULT "%d\n", file, complineno);
    va_end(args);
}
#endif

void ac_flush_transients(CompilerBundle *cb)
{
    hst_for_each(&cb->transients, ac_decr_transients, cb);
    hst_for_each(&cb->loadedTransients, ac_decr_loaded_transients, cb);

    hst_free(&cb->transients);
    hst_free(&cb->loadedTransients);

    cb->transients = hst_create();
    cb->loadedTransients = hst_create();
}

ExportControl *ac_get_exports(AST *ast)
{
    ExportControl *ec = ec_alloc();

    for(; ast; ast = ast->next)
    {
        if(ast->type != AEXPORT)
            continue;

        ASTExportSymbol *aex = (ASTExportSymbol *)ast;
        ec_add_wcard(ec, aex->fmt, aex->kind);
    }

    return ec;
}

LLVMModuleRef ac_compile(AST *ast, int include_rc)
{
    CompilerBundle cb;
    cb.module = LLVMModuleCreateWithNameInContext("main-module", utl_get_current_context());
    cb.builder = LLVMCreateBuilderInContext(utl_get_current_context());
    cb.transients = hst_create();
    cb.loadedTransients = hst_create();
    cb.genericFunctions = hst_create();
    cb.genericWorkList = arr_create(5);
    cb.nextCaseBlock = NULL;

    cb.compilingMethod = 0;
    cb.inDeferment = 0;

    cb.currentLoopEntry = cb.currentLoopExit = NULL;

    cb.td = LLVMCreateTargetData("");

    VarScopeStack vs = vs_make();
    vs.deferCallback = &ac_deferment_callback;
    cb.varScope = &vs;
    cb.enum_lookup = NULL;

    cb.exports = ac_get_exports(ast);

    vs_push(cb.varScope);

    if(include_rc)
        ac_prepare_module(cb.module);
    LLVMTypeRef param_types[] = {LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0)};
    LLVMTypeRef func_type = LLVMFunctionType(LLVMInt32TypeInContext(utl_get_current_context()), param_types, 1, 1);
    LLVMAddFunction(cb.module, "printf", func_type);

    the_module = cb.module;

    AST *old = ast;
    for(; ast; ast = ast->next)
    {
        ac_add_early_name_declaration(ast, &cb);
        ac_add_global_variable_declarations(ast, &cb);
    }
    ast = old;
    for(; ast; ast = ast->next)
        ac_add_early_declarations(ast, &cb);

    vs_put(cb.varScope, (char *)"__egl_millis", LLVMGetNamedFunction(cb.module, "__egl_millis"), ett_function_type(ett_base_type(ETInt64), NULL, 0), -1);

    ast = old;

    ac_check_generics(&cb);

    ac_make_enum_definitions(ast, &cb);
    ac_make_struct_definitions(ast, &cb);
    ac_generate_interface_definitions(ast, &cb);
    ac_make_class_definitions(ast, &cb);

    for(; ast; ast = ast->next)
    {
        ac_dispatch_declaration(ast, &cb);
    }

    ac_compile_generics(&cb);

    vs_pop(cb.varScope);

    LLVMDisposeBuilder(cb.builder);
    LLVMDisposeTargetData(cb.td);
    vs_free(cb.varScope);

    ac_generics_cleanup(&cb);

    hst_free(&cb.transients);
    hst_free(&cb.loadedTransients);
    hst_free(&cb.genericFunctions);

    arr_free(&cb.genericWorkList);

    ec_free(cb.exports);

    return cb.module;
}

void ac_prepare_module(LLVMModuleRef module)
{
    LLVMTypeRef param_types_rc[] = {LLVMPointerType(LLVMInt64TypeInContext(utl_get_current_context()), 0)};
    LLVMTypeRef func_type_rc = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), param_types_rc, 1, 0);
    LLVMAddFunction(module, "__egl_incr_ptr", func_type_rc);
    LLVMAddFunction(module, "__egl_decr_ptr", func_type_rc);
    LLVMAddFunction(module, "__egl_decr_no_free", func_type_rc);
    LLVMAddFunction(module, "__egl_check_ptr", func_type_rc);
    LLVMAddFunction(module, "__egl_prepare", func_type_rc);

    LLVMTypeRef param_types_we[] = { LLVMPointerType(LLVMInt64TypeInContext(utl_get_current_context()), 0), LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0)};
    func_type_rc = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), param_types_we, 2, 0);
    LLVMAddFunction(module, "__egl_add_weak", func_type_rc);

    LLVMTypeRef ty = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    func_type_rc = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), &ty, 1, 0);
    LLVMAddFunction(module, "__egl_remove_weak", func_type_rc);

    LLVMTypeRef param_types_arr1[] = {LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), LLVMInt64TypeInContext(utl_get_current_context())};
    func_type_rc = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), param_types_arr1, 2, 0);
    LLVMAddFunction(module, "__egl_array_fill_nil", func_type_rc);

    param_types_arr1[0] = LLVMPointerType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), 0);
    func_type_rc = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), param_types_arr1, 2, 0);
    LLVMAddFunction(module, "__egl_array_decr_ptrs", func_type_rc);

    func_type_rc = LLVMFunctionType(LLVMInt64TypeInContext(utl_get_current_context()), NULL, 0, 0);
    LLVMAddFunction(module, "__egl_millis", func_type_rc);


    LLVMTypeRef param_types_destruct[2];
    param_types_destruct[0] = LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0);
    param_types_destruct[1] = LLVMInt1TypeInContext(utl_get_current_context());
    func_type_rc = LLVMFunctionType(LLVMVoidTypeInContext(utl_get_current_context()), param_types_destruct, 2, 0);
    LLVMAddFunction(module, "__egl_counted_destructor", func_type_rc);

    LLVMTypeRef lookup_types[] = {LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), LLVMInt32TypeInContext(utl_get_current_context())};
    func_type_rc = LLVMFunctionType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), lookup_types, 3, 0);
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

void ac_add_global_variable_declarations(AST *ast, CompilerBundle *cb)
{
    if(ast->type != AVARDECL)
        return;

    ASTVarDecl *a = (ASTVarDecl *)ast;
    ASTTypeDecl *t = (ASTTypeDecl *)a->atype;

    EagleComplexType *et = t->etype;

    LLVMValueRef glob = LLVMAddGlobal(cb->module, ett_llvm_type(et), a->ident); 

    LLVMValueRef init = NULL;
    if(a->staticInit)
    {
        init = ac_dispatch_constant(a->staticInit, cb);
        EagleComplexType *f = a->staticInit->resultantType;
        if(!ett_are_same(f, et))
        {
            init = ac_convert_const(init, et, f);
            if(!init)
                die(ALN, "Invalid implicit constant conversion");
        }
    }
    else if(a->linkage != VLExternal)
    {
        init = ett_default_value(et);
        if(!init)
            die(ALN, "Variable %s does not have a valid static type", a->ident);
    }

    LLVMSetInitializer(glob, init);

    vs_put(cb->varScope, a->ident, glob, et, ALN);

    if(!init || a->linkage == VLExport ||
       ec_allow(cb->exports, a->ident, TSTATIC))
    {
        // If there is no initializer, this is a variable declaration
        // (i.e. extern variable ...). Thus, we need to ensure we
        // don't have unused variable warnings.
        VarBundle *bun = vs_get(cb->varScope, a->ident);
        bun->wasassigned = bun->wasused = 1;
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
    EagleComplexType *eparam_types[ct];
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
        EagleComplexType *ftype = ett_function_type(retType->etype, eparam_types, ct);
        ((EagleFunctionType *)ftype)->variadic = 1;
        vs_put(cb->varScope, a->ident, func, ftype, -1);
        return;
    }
    if(strcmp(a->ident, "free") == 0)
    {
        LLVMValueRef func = LLVMGetNamedFunction(cb->module, "free");
        if(func)
        {
            vs_put(cb->varScope, a->ident, func, ett_function_type(retType->etype, eparam_types, ct), -1);
            return;
        }
    }

    EagleComplexType *ftype = ett_function_type(retType->etype, eparam_types, ct);
    ((EagleFunctionType *)ftype)->variadic = a->vararg;

    LLVMValueRef func = NULL;

    if(ac_decl_is_generic(ast))
    {
        ac_generic_register(ast, ftype, cb);
    }
    else
    {
        LLVMTypeRef func_type = LLVMFunctionType(ett_llvm_type(retType->etype), param_types, ct, a->vararg);
        func = LLVMAddFunction(cb->module, a->ident, func_type);

        if(retType->etype->type == ETStruct)
            die(ALN, "Returning struct by value not supported. (%s)\n", a->ident);
    }

    if(!ec_allow(cb->exports, a->ident, TFUNC) && a->body && strcmp(a->ident, "main") && a->linkage != VLExport)
    {
        if(func)
            LLVMSetLinkage(func, LLVMPrivateLinkage);
    }
    else
    {
        ec_register_record(cb->exports, a->ident, TFUNC);
    }

    vs_put(cb->varScope, a->ident, func, ftype, -1);
}

LLVMValueRef ac_dispatch_constant(AST *ast, CompilerBundle *cb)
{
    LLVMValueRef val = NULL;
    switch(ast->type)
    {
        case AVALUE:
            val = ac_const_value(ast, cb);
            break;
        default:
            die(ALN, "Invalid constant type.");
            return NULL;
    }

    if(!ast->resultantType)
        die(ALN, "Internal Error. AST Resultant Type for expression not set.");

    return val;
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
        case ATYPELOOKUP:
            val = ac_compile_type_lookup(ast, cb);
            break;
        case ATERNARY:
            val = ac_compile_ternary(ast, cb);
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
        case ATYPELOOKUP:
            ac_compile_type_lookup(ast, cb);
            break;
        case ASWITCH:
            ac_compile_switch(ast, cb);
            break;
        case ATERNARY:
            ac_compile_ternary(ast, cb);
            break;
        case ADEFER:
            ac_guard_deferment(cb, ALN);
            vs_add_deferment(cb->varScope, ((ASTDefer *)ast)->block);
            break;
        default:
            die(ALN, "Invalid statement type.");
    }
    
    ac_flush_transients(cb);
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
        case AENUMDECL:
        case AVARDECL:
        case AEXPORT:
            break;
        case AGENDECL:
            ac_compile_generator_code(ast, cb);
            break;
        default:
            die(ALN, "Invalid declaration type.");
            return;
    }
}

typedef struct
{
    DispatchObserver observer;
    void *data;
} DispatchObserverBundle;

void ac_add_dispatch_observer(CompilerBundle *cb, ASTType type, DispatchObserver observer, void *data)
{
    DispatchObserverBundle *db = malloc(sizeof(DispatchObserverBundle));
    db->observer = observer;
    db->data = data;

    hst_put(&cb->dispatchObservers, (void *)type, db, ahhd, ahed);
}

void ac_remove_dispatch_observer(CompilerBundle *cb, ASTType type)
{
    DispatchObserverBundle *db = hst_get(&cb->dispatchObservers, (void *)type, ahhd, ahed);
    if(db)
        free(db);
}

void ac_guard_deferment(CompilerBundle *cb, int lineno)
{
    if(cb->inDeferment)
        die(lineno, "Invalid statement in deferment");
}

static void ac_deferment_callback(AST *ast, void *data)
{
    CompilerBundle *cb = data;
    cb->inDeferment = 1;

    for(; ast; ast = ast->next)
    {
        ac_dispatch_statement(ast, cb);
    }

    cb->inDeferment = 0;
}

