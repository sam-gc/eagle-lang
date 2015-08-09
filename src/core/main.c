#include <stdio.h>
#include "compiler/ast_compiler.h"

extern char *yytext;

extern int yyparse();
extern int yylex();

extern AST *ast_root;

int main(int argc, const char * argv[])
{
    /*
    */
    etTargetData = LLVMCreateTargetData("");

    yyparse();

    LLVMModuleRef module = ac_compile(ast_root);
    LLVMDumpModule(module);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    LLVMDisposeModule(module);

    LLVMDisposeTargetData(etTargetData);
    
//    int token;
//    while ((token = yylex()) != 0)
//        printf("Token: %d (%s)\n", token, yytext);
//    return 0;
    
    return 0;
}
