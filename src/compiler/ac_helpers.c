#include "ast_compiler.h"

long ahhd(void *k, void *d)
{
    return (long)k;
}

int ahed(void *k, void *d)
{
    return k == d;
}


LLVMValueRef ac_build_conversion(LLVMBuilderRef builder, LLVMValueRef val, EagleTypeType *from, EagleTypeType *to)
{
    switch(from->type)
    {
        case ETPointer:
        {
            if(to->type != ETPointer)
                die(-1, "Non-pointer type may not be converted to pointer type.");

            if(ett_get_base_type(to) == ETAny)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
            if(ett_get_base_type(from) == ETAny && ett_pointer_depth(from) == 1)
                return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
            if(ET_IS_COUNTED(to) != ET_IS_COUNTED(from) || ET_IS_WEAK(to) != ET_IS_WEAK(from))
                die(-1, "Counted pointer type may not be converted to counted pointer type. Use the \"unwrap\" keyword.");

            if(ett_pointer_depth(to) != ett_pointer_depth(from))
                die(-1, "Implicit pointer conversion invalid. Cannot conver pointer of depth %d to depth %d.", ett_pointer_depth(to), ett_pointer_depth(from));
            if(ett_get_base_type(to) != ett_get_base_type(from))
                die(-1, "Implicit pointer conversion invalid; pointer types are incompatible.");

            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETArray:
        {
            if(to->type != ETPointer && to->type != ETArray)
                die(-1, "Arrays may only be converted to equivalent pointers.");

            //LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
            return LLVMBuildBitCast(builder, val, ett_llvm_type(to), "ptrtmp");
        }
        case ETInt1:
        case ETInt8:
        case ETInt32:
        case ETInt64:
            switch(to->type)
            {
                case ETInt1:
                    return LLVMBuildICmp(builder, LLVMIntNE, val, LLVMConstInt(ett_llvm_type(from), 0, 0), "cmp");
                case ETInt8:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildIntCast(builder, val, ett_llvm_type(to), "conv");
                case ETDouble:
                    return LLVMBuildSIToFP(builder, val, LLVMDoubleType(), "conv");
                default:
                    die(-1, "Invalid implicit conversion.");
                    break;
            }
        case ETDouble:
            switch(to->type)
            {
                case ETDouble:
                    return val;
                case ETInt1:
                    return LLVMBuildFCmp(builder, LLVMRealONE, val, LLVMConstReal(0, 0), "cmp");
                case ETInt8:
                case ETInt32:
                case ETInt64:
                    return LLVMBuildFPToSI(builder, val, ett_llvm_type(to), "conv");
                default:
                    die(-1, "Invalid implicit conversion from double.");
                    break;
            }
            break;
        default:
            die(-1, "Invalid implicit conversion.");
            break;
    }

    return NULL;
}

LLVMValueRef ac_make_add(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFAdd(builder, left, right, "addtmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildAdd(builder, left, right, "addtmp");
        default:
            die(-1, "The given types may not be summed.");
            return NULL;
    }
}

LLVMValueRef ac_make_sub(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFSub(builder, left, right, "subtmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSub(builder, left, right, "subtmp");
        default:
            die(-1, "The given types may not be subtracted.");
            return NULL;
    }
}

LLVMValueRef ac_make_mul(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFMul(builder, left, right, "multmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildMul(builder, left, right, "multmp");
        default:
            die(-1, "The given types may not be multiplied.");
            return NULL;
    }
}

LLVMValueRef ac_make_div(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type)
{
    switch(type)
    {
        case ETDouble:
            return LLVMBuildFDiv(builder, left, right, "divtmp");
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildSDiv(builder, left, right, "divtmp");
        default:
            die(-1, "The given types may not be divided.");
            return NULL;
    }
}

LLVMValueRef ac_make_comp(LLVMValueRef left, LLVMValueRef right, LLVMBuilderRef builder, EagleType type, char comp)
{
    LLVMIntPredicate ip;
    LLVMRealPredicate rp;

    switch(comp)
    {
        case 'e':
            ip = LLVMIntEQ;
            rp = LLVMRealOEQ;
            break;
        case 'n':
            ip = LLVMIntNE;
            rp = LLVMRealONE;
            break;
        case 'g':
            ip = LLVMIntSGT;
            rp = LLVMRealOGT;
            break;
        case 'l':
            ip = LLVMIntSLT;
            rp = LLVMRealOLT;
            break;
        case 'G':
            ip = LLVMIntSGE;
            rp = LLVMRealOGE;
            break;
        case 'L':
            ip = LLVMIntSLE;
            rp = LLVMRealOLE;
            break;
        default:
            ip = LLVMIntEQ;
            rp = LLVMRealOEQ;
            break;
    }

    switch(type)
    {
        case ETDouble:
            return LLVMBuildFCmp(builder, rp, left, right, "eqtmp");
        case ETInt1:
        case ETInt8:
        case ETInt32:
        case ETInt64:
            return LLVMBuildICmp(builder, ip, left, right, "eqtmp");
        default:
            die(-1, "The given types may not be compared.");
            return NULL;
    }
}

void ac_replace_with_counted(CompilerBundle *cb, VarBundle *b)
{

    LLVMBasicBlockRef isb = LLVMGetInsertBlock(cb->builder);
    LLVMValueRef val = b->value;
    LLVMPositionBuilderBefore(cb->builder, val);

    LLVMValueRef mal = ac_compile_malloc_counted(b->type, &b->type, val, cb);
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type(b->type), "");
    LLVMBuildStore(cb->builder, mal, pos);
    ac_incr_pointer(cb, &pos, b->type);

    ((EaglePointerType *)b->type)->closed = 1;

    b->value = pos;
    b->scopeCallback = ac_scope_leave_callback;
    b->scopeData = cb;

    LLVMValueRef to = LLVMBuildStructGEP(cb->builder, mal, 5, "");
    LLVMReplaceAllUsesWith(val, to);
    LLVMInstructionEraseFromParent(val);

    LLVMPositionBuilderAtEnd(cb->builder, isb);
}
