/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ast_compiler.h"

LLVMValueRef ac_const_value(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    switch(a->etype)
    {
        case ETInt1:
            a->resultantType = ett_base_type(ETInt1);
            return LLVMConstInt(LLVMInt1TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETInt8:
        case ETUInt8:
            a->resultantType = ett_base_type(a->etype);
            return LLVMConstInt(LLVMInt8TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETInt16:
        case ETUInt16:
            a->resultantType = ett_base_type(a->etype);
            return LLVMConstInt(LLVMInt16TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETInt32:
        case ETUInt32:
            a->resultantType = ett_base_type(a->etype);
            return LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETInt64:
        case ETUInt64:
            a->resultantType = ett_base_type(a->etype);
            return LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), a->value.i, 1);
        case ETDouble:
            a->resultantType = ett_base_type(ETDouble);
            return LLVMConstReal(LLVMDoubleTypeInContext(utl_get_current_context()), a->value.d);
        case ETCString:
        {
            a->resultantType = ett_pointer_type(ett_base_type(ETInt8));
            LLVMValueRef conststr = LLVMConstStringInContext(utl_get_current_context(), a->value.id, strlen(a->value.id), 0);
            
            LLVMValueRef globstr = LLVMAddGlobal(cb->module, LLVMTypeOf(conststr), ".str");
            LLVMSetInitializer(globstr, conststr);
            LLVMSetThreadLocal(globstr, 0);
            LLVMSetLinkage(globstr, LLVMPrivateLinkage);
            LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(utl_get_current_context()), 0, 1);
            return LLVMConstGEP(globstr, &zero, 1);
            //LLVMValueRef ptr = LLVMBuildGlobalStringPtr(cb->builder, a->value.id, "str");
            
        }
        case ETNil:
            a->resultantType = ett_pointer_type(ett_base_type(ETAny));
            return LLVMConstPointerNull(ett_llvm_type(a->resultantType));
        default:
            die(ALN, msgerr_unknown_value_type);
            return NULL;
    }
}

LLVMValueRef ac_const_struct_lit(AST *ast, CompilerBundle *cb)
{
    ASTStructLit *asl = (ASTStructLit *)ast;
    if(!asl->name)
        die(ALN, msgerr_struct_literal_type_unknown);

    Arraylist *names;
    Arraylist *types;
    Hashtable *dict = &asl->exprs;

    EagleComplexType *st = ett_struct_type(asl->name);

    ty_struct_get_members(st, &names, &types);
    LLVMValueRef vals[names->count];

    for(int i = 0; i < names->count; i++)
    {
        char *name = names->items[i];
        EagleComplexType *type = types->items[i];

        AST *e = hst_get(dict, name, NULL, NULL);
        if(!e)
        {
            vals[i] = ett_default_value(types->items[i]);
            if(!vals[i])
                die(ALN, msgerr_invalid_constant_struct_member);
            continue;
        }

        LLVMValueRef it = ac_dispatch_constant(e, cb);

        if(!ett_are_same(e->resultantType, type))
        {
            it = ac_convert_const(it, type, e->resultantType);
            if(!it)
                die(ALN, msgerr_invalid_conversion_constant);
        }

        vals[i] = it;
    }

    ast->resultantType = st;
    return LLVMConstNamedStruct(ett_llvm_type(st), vals, names->count);
}

LLVMValueRef ac_const_enum(AST *ast, CompilerBundle *cb)
{
    ASTTypeLookup *a = (ASTTypeLookup *)ast;

    if(!ty_is_enum(a->name))
        die(ALN, msgerr_invalid_type_lookup_type, a->name);

    EagleComplexType *et = ett_enum_type(a->name);
    int valid;
    long enum_val = ty_lookup_enum_item(et, a->item, &valid);

    if(!valid)
        die(ALN, msgerr_item_not_in_enum, a->name, a->item);

    a->resultantType = et;

    return LLVMConstInt(LLVMInt64TypeInContext(utl_get_current_context()), enum_val, 0);
}

LLVMValueRef ac_const_identifier(AST *ast, CompilerBundle *cb)
{
    ASTValue *a = (ASTValue *)ast;
    VarBundle *b = vs_get(cb->varScope, a->value.id);

    if(!b && cb->enum_lookup)
    {
        LLVMValueRef enl = ac_lookup_enum(cb->enum_lookup, a->value.id);
        if(!enl)
            die(ALN, msgerr_item_not_in_enum, a->value.id, ((EagleEnumType *)cb->enum_lookup)->name);
        a->resultantType = cb->enum_lookup;
        return enl;
    }

    if(!b)
        die(ALN, msgerr_undeclared_identifier, a->value.id);
    if(b->type->type == ETAuto)
        die(ALN, msgerr_unknown_var_read, a->value.id);

    b->wasused = 1;

    a->resultantType = b->type;

    return LLVMGetInitializer(b->value);
}

LLVMValueRef ac_convert_const(LLVMValueRef val, EagleComplexType *to, EagleComplexType *from)
{
    switch(from->type)
    {
        case ETInt1:
        case ETInt8:
        case ETInt16:
        case ETInt32:
        case ETInt64:
        case ETUInt8:
        case ETUInt16:
        case ETUInt32:
        case ETUInt64:
        {
            switch(to->type)
            {
                case ETInt1:
                    return LLVMConstICmp(LLVMIntNE, val, LLVMConstInt(ett_llvm_type(from), 0, 0));
                case ETInt8:
                case ETInt16:
                case ETInt32:
                case ETInt64:
                    return LLVMConstIntCast(val, ett_llvm_type(to), 1);
                case ETUInt8:
                case ETUInt16:
                case ETUInt32:
                case ETUInt64:
                    return LLVMConstIntCast(val, ett_llvm_type(to), 0);
                case ETDouble:
                    return LLVMConstSIToFP(val, ett_llvm_type(to));
                default:
                    return NULL;
            }
        }
        case ETDouble:
        {
            switch(to->type)
            {
                case ETDouble:
                    return val;
                case ETInt1:
                    return LLVMConstFCmp(LLVMRealONE, val, LLVMConstReal(0, 0));
                case ETInt8:
                case ETInt16:
                case ETInt32:
                case ETInt64:
                    return LLVMConstFPToSI(val, ett_llvm_type(to));
                case ETUInt8:
                case ETUInt16:
                case ETUInt32:
                case ETUInt64:
                    return LLVMConstFPToUI(val, ett_llvm_type(to));
                default:
                    return NULL;
            }
        }
        default:
            return NULL;
    }
}

