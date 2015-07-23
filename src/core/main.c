//
//  main.c
//  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#include <stdio.h>
#include "compiler/ast_compiler.h"

extern char *yytext;

extern int yyparse();
extern int yylex();

extern AST *ast_root;

int main(int argc, const char * argv[]) {
    // insert code here...
    printf("Hello, World!\n");
    
    yyparse();

    LLVMModuleRef module = ac_compile(ast_root);
    LLVMDumpModule(module);
    LLVMDisposeModule(module);
    
//    int token;
//    while ((token = yylex()) != 0)
//        printf("Token: %d (%s)\n", token, yytext);
//    return 0;
    
    return 0;
}
