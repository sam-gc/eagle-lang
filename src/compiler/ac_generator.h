#ifndef AC_GENERATOR_H
#define AC_GENERATOR_H

#include "core/arraylist.h"
#include "core/hashtable.h"

typedef struct {
    arraylist context_types;
    hashtable *param_mapping;
    hashtable *params;
    char *ident;
    LLVMValueRef func;
    LLVMTypeRef contextType;

    EagleTypeType **eparam_types;
    int epct;
} GeneratorBundle;

void ac_generator_replace_allocas(CompilerBundle *cb, GeneratorBundle *gb);
void ac_compile_generator_code(AST *ast, CompilerBundle *cb);
void ac_add_gen_declaration(AST *ast, CompilerBundle *cb);

#endif
