#include <stdio.h>
#include "imports.h"
#include "compiler/ast.h"
#include "grammar/eagle.tab.h"
#define YY_BUF_SIZE 32768


typedef void* YY_BUFFER_STATE;

extern char *yytext;

extern int skip_type_check;
extern FILE *yyin;
extern int yylex();
extern int yylineno();
extern YY_BUFFER_STATE yy_create_buffer(FILE*, size_t);
extern void yypush_buffer_state(YY_BUFFER_STATE);
extern void yypop_buffer_state();

void imp_scan_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    skip_type_check = 1;
    YY_BUFFER_STATE buf = yy_create_buffer(f, YY_BUF_SIZE);
    yypush_buffer_state(buf);
    int token;
    
    int bracket_depth = 0;
    int in_class = 0;
    while((token = yylex()) != 0)
    {
        if(token == TLBRACE)
            bracket_depth++;
        if(token == TRBRACE)
        {
            bracket_depth--;
            if(bracket_depth == 0 && in_class)
            {
                in_class = 0;
                printf("} ");
            }
        }

        if(token == TCLASS || token == TSTRUCT || token == TINTERFACE)
        {
            in_class = 1;
        }

        if(bracket_depth > (in_class ? 1 : 0) || token == TRBRACE)
            continue;

        printf("%s ", yytext);
    }

    yypop_buffer_state();
    fclose(f);
    skip_type_check = 0;
}
