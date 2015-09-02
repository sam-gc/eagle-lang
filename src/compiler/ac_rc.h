#ifndef AC_RC_H
#define AC_RC_H

void ac_scope_leave_callback(LLVMValueRef pos, EagleTypeType *ty, void *data);
void ac_scope_leave_array_callback(LLVMValueRef pos, EagleTypeType *ty, void *data);
void ac_scope_leave_weak_callback(LLVMValueRef pos, EagleTypeType *ty, void *data);
void ac_decr_loaded_transients(void *key, void *val, void *data);
void ac_decr_transients(void *key, void *val, void *data);
void ac_unwrap_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleTypeType *ty, int keepptr);
void ac_prepare_pointer(CompilerBundle *cb, LLVMValueRef ptr, EagleTypeType *ty);
void ac_incr_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleTypeType *ty);
void ac_incr_val_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleTypeType *ty);
void ac_check_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleTypeType *ty);
void ac_decr_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleTypeType *ty);
void ac_decr_val_pointer(CompilerBundle *cb, LLVMValueRef *ptr, EagleTypeType *ty);
void ac_decr_in_array(CompilerBundle *cb, LLVMValueRef arr, int ct);
void ac_nil_fill_array(CompilerBundle *cb, LLVMValueRef arr, int ct);
void ac_add_weak_pointer(CompilerBundle *cb, LLVMValueRef ptr, LLVMValueRef weak, EagleTypeType *ty);
void ac_remove_weak_pointer(CompilerBundle *cb, LLVMValueRef weak, EagleTypeType *ty);

#endif
