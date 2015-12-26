#include <stdio.h>
#include "imports.h"
#include "compiler/ast.h"
#include "grammar/eagle.tab.h"
#include "core/stringbuilder.h"
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

char *imp_scan_file(const char *filename)
{
    Strbuilder strb;
    sb_init(&strb);
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
                sb_append(&strb, "} ");
            }
        }

        if(token == TCLASS || token == TSTRUCT || token == TINTERFACE)
        {
            in_class = 1;
        }

        if(bracket_depth > (in_class ? 1 : 0) || token == TRBRACE || token == TEXTERN)
            continue;

        if(token == TFUNC)
            sb_append(&strb, "extern ");

        sb_append(&strb, yytext);
        sb_append(&strb, " ");
    }

    yypop_buffer_state();
    fclose(f);
    skip_type_check = 0;

    return strb.buffer;
}
