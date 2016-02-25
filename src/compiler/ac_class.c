#include "ast_compiler.h"
#include "core/config.h"
#include "core/utils.h"
#include <string.h>

void ac_add_class_declaration(AST *ast, CompilerBundle *cb)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    LLVMStructCreateNamed(utl_get_current_context(), a->name);
    
}

char *ac_gen_method_name(char *class_name, char *method)
{
    char *name = malloc(strlen(class_name) + strlen(method) + 1);
    sprintf(name, "%s%s", class_name, method);
    return name;
}

char *ac_gen_vtable_name(char *class_name)
{
    char *name = malloc(strlen(class_name) + 12);
    sprintf(name, "__%s_vtable", class_name);
    return name;
}

void ac_generate_interface_methods_each(void *key, void *val, void *data)
{
    ASTClassDecl *cd = (ASTClassDecl *)data;
    ASTFuncDecl *fd = key;
    EagleTypeType *ety = val;

    // ASTFuncDecl *fd = val;
    // EagleFunctionType *ety = (EagleFunctionType *)((EaglePointerType *)arr_get(&cd->types, idx - 1))->to;

    ty_add_interface_method(cd->name, fd->ident, ety);
}

void ac_generate_interface_definitions(AST *ast, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type != AIFCDECL)
            continue;

        ASTClassDecl *a = (ASTClassDecl *)ast;

        hst_for_each(&a->method_types, ac_generate_interface_methods_each, a);
    }
}

void ac_prepare_methods_each(void *key, void *val, void *data)
{
    ac_class_helper *h = (ac_class_helper *)data;
    ASTClassDecl *cd = (ASTClassDecl *)h->ast;

    ASTFuncDecl *fd = key;
    EagleFunctionType *ety = val;

    ASTVarDecl *vd = (ASTVarDecl *)fd->params;
    vd->atype = ast_make_counted(ast_make_pointer(ast_make_type(cd->name)));

    char *method_name = ac_gen_method_name(cd->name, fd->ident);

    ety->params[0] = ett_pointer_type(ett_struct_type(cd->name));
    ((EaglePointerType *)ety->params[0])->counted = 1;
    LLVMTypeRef ft = ett_llvm_type((EagleTypeType *)ety);
    LLVMAddFunction(h->cb->module, method_name, ft);

    ty_add_method(cd->name, fd->ident, (EagleTypeType *)ety);

    free(method_name);
}

void ac_class_add_early_definitions(ASTClassDecl *cd, CompilerBundle *cb, ac_class_helper *h)
{
    hst_for_each(&cd->method_types, ac_prepare_methods_each, h);
    ASTFuncDecl *fd = (ASTFuncDecl *)cd->initdecl;
    if(!fd)
        return;

    ASTVarDecl *vd = (ASTVarDecl *)fd->params;
    vd->atype = ast_make_counted(ast_make_pointer(ast_make_type(cd->name)));
    char *method_name = ac_gen_method_name(cd->name, (char *)"__init__");
    
    EagleFunctionType *ety = (EagleFunctionType *)cd->inittype;
    ety->params[0] = ett_pointer_type(ett_struct_type(cd->name));
    ((EaglePointerType *)ety->params[0])->counted = 1;
    LLVMTypeRef ft = ett_llvm_type((EagleTypeType *)ety);
    LLVMAddFunction(cb->module, method_name, ft);

    //ty_add_method(cd->name, fd->ident, (EagleTypeType *)ety);
    ty_add_init(cd->name, (EagleTypeType *)ety);

    free(method_name);
}

void ac_compile_class_init(ASTClassDecl *cd, CompilerBundle *cb)
{
    ASTFuncDecl *fd = (ASTFuncDecl *)cd->initdecl;
    if(!fd)
        return;

    char *method_name = ac_gen_method_name(cd->name, (char *)"__init__");
    
    LLVMValueRef func = LLVMGetNamedFunction(cb->module, method_name);

    free(method_name);
    EagleFunctionType *ety = (EagleFunctionType *)cd->inittype;

    if(!cd->ext)
        ac_compile_function_ex((AST *)fd, cb, func, ety);

    ety->params[0] = ett_pointer_type(ett_base_type(ETAny));
}

void ac_compile_class_destruct(ASTClassDecl *cd, CompilerBundle *cb)
{
    ASTFuncDecl *fd = (ASTFuncDecl *)cd->destructdecl;
    if(!fd)
        return;

    ASTVarDecl *vd = (ASTVarDecl *)fd->params;
    vd->atype = ast_make_counted(ast_make_pointer(ast_make_type(cd->name)));
    char *method_name = ac_gen_method_name(cd->name, (char *)"__destruct__");
    
    EagleFunctionType *ety = (EagleFunctionType *)cd->destructtype;
    ety->params[0] = ett_pointer_type(ett_struct_type(cd->name));
    ((EaglePointerType *)ety->params[0])->counted = 1;
    LLVMTypeRef ft = ett_llvm_type((EagleTypeType *)ety);
    LLVMValueRef func = LLVMAddFunction(cb->module, method_name, ft);

    free(method_name);

    if(!cd->ext)
        ac_compile_function_ex((AST *)fd, cb, func, ety);

    ety->params[0] = ett_pointer_type(ett_base_type(ETAny));
}

void ac_check_and_register_implementation(char *method, ac_class_helper *h, char *class, LLVMValueRef func, ASTClassDecl *cd)
{
    int i, sidx;
    arraylist *ifcs = &cd->interfaces;
    for(i = sidx = 0; i < ifcs->count; sidx += ty_interface_count(arr_get(ifcs, i)), i++)
    {
        char *name = arr_get(ifcs, i);
        int offset = ty_interface_offset(name, method);
        if(offset < 0)
            continue;

        EagleTypeType *implty = ty_method_lookup(name, method);
        EagleTypeType *clsty  = ty_method_lookup(class, method);

        if(!ett_are_same(implty, clsty))
            die(cd->lineno, "Implementation of method (%s) in class (%s) does not match interface", method, class);

#ifdef llvm_OLD
        func = LLVMConstBitCast(func, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0));
#endif

        h->interface_pointers[sidx + offset] = func;
        return;
    }
}

void ac_compile_class_methods_each(void *key, void *val, void *data)
{
    ac_class_helper *h = (ac_class_helper *)data;
    ASTClassDecl *cd = (ASTClassDecl *)h->ast;

    ASTFuncDecl *fd = key;
    EagleFunctionType *ety = val;

    char *method_name = ac_gen_method_name(cd->name, fd->ident);

    LLVMValueRef func = LLVMGetNamedFunction(h->cb->module, method_name);

    free(method_name);

    h->cb->compilingMethod = 1;
    if(!cd->ext)
        ac_compile_function_ex((AST *)fd, h->cb, func, ety);
    h->cb->compilingMethod = 0;

    ety->params[0] = ett_pointer_type(ett_base_type(ETAny));

    if(cd->interfaces.count && !cd->ext)
        ac_check_and_register_implementation(fd->ident, h, cd->name, func, cd);
}

void ac_make_class_definitions(AST *ast, CompilerBundle *cb)
{
    AST *old = ast;
    for(; ast; ast = ast->next)
    {
        if(ast->type != ACLASSDECL)
            continue;

        ASTClassDecl *a = (ASTClassDecl *)ast;

        LLVMTypeRef *tys = malloc(sizeof(LLVMTypeRef) * (a->types.count + 1));
        int i;
        for(i = 0; i < a->types.count; i++)
            tys[i + 1] = ett_llvm_type(arr_get(&a->types, i));

        tys[0] = LLVMPointerType(ty_class_indirect(), 0);

        LLVMTypeRef loaded = LLVMGetTypeByName(cb->module, a->name);
        LLVMStructSetBody(loaded, tys, a->types.count + 1, 0);
        free(tys);

        ty_add_struct_def(a->name, &a->names, &a->types);
        ett_class_set_interfaces(ett_class_type(a->name), &a->interfaces);
    }

    ast = old;
    for(; ast; ast = ast->next)
    {
        if(ast->type != ACLASSDECL)
            continue;

        ASTClassDecl *a = (ASTClassDecl *)ast;

        ac_class_helper h;
        h.ast = ast;
        h.cb = cb;

        LLVMTypeRef indirtype = ty_class_indirect();
        LLVMValueRef vtable = NULL;

        LLVMValueRef constnames = NULL;
        LLVMValueRef offsets = NULL;
        if(a->interfaces.count && !a->ext)
        {
            int count = (int)a->interfaces.count;
            LLVMValueRef constnamesarr[a->interfaces.count];
            LLVMValueRef offsetsarr[a->interfaces.count];
            int curoffset = 0;
            int i;
            for(i = 0; i < (int)a->interfaces.count; i++)
            {
                char *interface = a->interfaces.items[i];
                constnamesarr[i] = ac_make_floating_string(cb, interface, "_ifc");

                // constnamesarr[i] = LLVMBuildGlobalStringPtr(cb->builder, a->interfaces.items[i], "iname");
                offsetsarr[i] = LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), curoffset, 0);
                curoffset += ty_interface_count(interface);
            }

            LLVMValueRef initcn = LLVMConstArray(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), constnamesarr, (int)a->interfaces.count);
            LLVMValueRef initof = LLVMConstArray(LLVMInt64TypeInContext(utl_get_current_context()), offsetsarr, (int)a->interfaces.count);

            constnames = LLVMAddGlobal(cb->module, LLVMArrayType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), count), "_ifn");
            offsets = LLVMAddGlobal(cb->module, LLVMArrayType(LLVMInt64TypeInContext(utl_get_current_context()), count), "_ifo");

            LLVMSetInitializer(constnames, initcn);
            LLVMSetInitializer(offsets, initof);
            h.interface_pointers = malloc(sizeof(LLVMValueRef) * curoffset);
            memset(h.interface_pointers, 0, sizeof(LLVMValueRef) * curoffset);
            h.table_len = curoffset;

            char *vtablename = ac_gen_vtable_name(a->name);
            vtable = LLVMAddGlobal(cb->module, indirtype, vtablename);
            free(vtablename);
        }

        h.vtable = vtable;

        ac_class_add_early_definitions(a, cb, &h);
        hst_for_each(&a->method_types, ac_compile_class_methods_each, &h);
        ac_compile_class_init(a, cb);
        ac_compile_class_destruct(a, cb);
        ac_make_class_constructor(ast, cb, &h);
        ac_make_class_destructor(ast, cb);

        if(a->interfaces.count && !a->ext)
        {
            int i;
            for(i = 0; i < h.table_len; i++) if(!h.interface_pointers[i])
                die(ast->lineno, "Class (%s) does not fully implement all announced interfaces", a->name);
            LLVMValueRef initptrs = LLVMConstArray(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), h.interface_pointers, h.table_len);
            LLVMValueRef ptrs = LLVMAddGlobal(cb->module, LLVMArrayType(LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), h.table_len), "_ifp");
            LLVMSetInitializer(ptrs, initptrs);
            free(h.interface_pointers);

            LLVMValueRef z = LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 0);
            LLVMValueRef indirvals[] = {
                LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), (int)a->interfaces.count, 0),
                LLVMConstGEP(constnames, &z, 1),
                LLVMConstGEP(offsets, &z, 1),
                LLVMConstGEP(ptrs, &z, 1)
            };

#ifdef llvm_OLD
            indirvals[1] = LLVMConstBitCast(indirvals[1], LLVMPointerType(LLVMPointerType(
                LLVMInt8TypeInContext(utl_get_current_context()), 0), 0));
            indirvals[2] = LLVMConstBitCast(indirvals[2], LLVMPointerType(LLVMInt64TypeInContext(utl_get_current_context()), 0));
            indirvals[3] = LLVMConstBitCast(indirvals[3], LLVMPointerType(LLVMPointerType(
                LLVMInt8TypeInContext(utl_get_current_context()), 0), 0));
#endif
            LLVMValueRef vtableinit = LLVMConstNamedStruct(indirtype, indirvals, 4);

            LLVMSetInitializer(vtable, vtableinit);
        }
        // if(ty_needs_destructor(ett_struct_type(a->name))) // {
        //     ac_make_struct_destructor(ast, cb);
        //     ac_make_struct_constructor(ast, cb);
        //     ac_make_struct_copy_constructor(ast, cb);
        // }
    }
}

void ac_make_class_constructor(AST *ast, CompilerBundle *cb, ac_class_helper *h)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    LLVMValueRef func = ac_gen_struct_constructor_func(a->name, cb, 0); // This just generates a name

    if(a->ext)
        return;

    EagleTypeType *ett = ett_pointer_type(ett_struct_type(a->name));

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    LLVMValueRef in = LLVMGetParam(func, 0);
    LLVMValueRef strct = LLVMBuildBitCast(cb->builder, in, ett_llvm_type(ett), "");

    LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, 0, "");

    LLVMBuildStore(cb->builder, h->vtable ? h->vtable : LLVMConstPointerNull(LLVMPointerType(ty_class_indirect(), 0)), gep);

    arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleTypeType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t) || ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i + 1, "");
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(t)), gep);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_constructor_func(((EagleStructType *)t)->name, cb, 0);
            LLVMValueRef param = LLVMBuildStructGEP(cb->builder, strct, i + 1, "");
            param = LLVMBuildBitCast(cb->builder, param, LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
            LLVMBuildCall(cb->builder, fc, &param, 1, "");
        }

        // LLVMValueRef meth = hst_get(&a->methods, (void *)(uintptr_t)(i + 1), ahhd, ahed);
        // if(meth)
        // {
        //     meth = LLVMBuildBitCast(cb->builder, meth, ett_llvm_type(t), "");
        //     LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i + 1, "");
        //     LLVMBuildStore(cb->builder, meth, gep);
        // }
    }

    LLVMBuildRetVoid(cb->builder);
}

void ac_make_class_destructor(AST *ast, CompilerBundle *cb)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    LLVMValueRef func = ac_gen_struct_destructor_func(a->name, cb);

    if(a->ext)
        return;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    EaglePointerType *ett = (EaglePointerType *)ett_pointer_type(ett_struct_type(a->name));
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type((EagleTypeType *)ett), "");

    if(a->destructdecl)
    {
        ett->counted = 1;
        LLVMValueRef me = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleTypeType *)ett), "");
        ett->counted = 0;
        char *destruct_name = ac_gen_method_name(a->name, (char *)"__destruct__");
        LLVMBuildCall(cb->builder, LLVMGetNamedFunction(cb->module, destruct_name), &me, 1, "");
        free(destruct_name);
    }

    LLVMValueRef cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, LLVMGetParam(func, 1), LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 1, 0), "");
    LLVMBasicBlockRef ifBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "if");
    LLVMBasicBlockRef elseBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "else");
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(utl_get_current_context(), func, "merge");
    LLVMBuildCondBr(cb->builder, cmp, ifBB, elseBB);
    LLVMPositionBuilderAtEnd(cb->builder, ifBB);

    ett->counted = 1;
    LLVMValueRef cast = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleTypeType *)ett), "");
    LLVMBuildStore(cb->builder, LLVMBuildStructGEP(cb->builder, cast, 5, ""), pos);
    LLVMBuildBr(cb->builder, mergeBB);

    ett->counted = 0;
    LLVMPositionBuilderAtEnd(cb->builder, elseBB);
    cast = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleTypeType *)ett), "");
    LLVMBuildStore(cb->builder, cast, pos);
    LLVMBuildBr(cb->builder, mergeBB);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
    pos = LLVMBuildLoad(cb->builder, pos, "");

    arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleTypeType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i + 1, "");
            ac_decr_pointer(cb, &gep, t);
        }
        else if(ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i + 1, "");
            ac_remove_weak_pointer(cb, gep, t);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_destructor_func(((EagleStructType *)t)->name, cb);
            LLVMValueRef params[2];
            params[0] = LLVMBuildBitCast(cb->builder, LLVMBuildStructGEP(cb->builder, pos, i + 1, ""), 
                    LLVMPointerType(LLVMInt8TypeInContext(utl_get_current_context()), 0), "");
            params[1] = LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), 0, 0);
            LLVMBuildCall(cb->builder, fc, params, 2, "");
        }
    }

    LLVMBuildRetVoid(cb->builder);
}
