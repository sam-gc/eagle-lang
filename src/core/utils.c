#include "utils.h"
#include "compiler/ast_compiler.h"
#include <string.h>

char *utl_gen_escaped_string(char *inp, int lineno)
{
    char *n = malloc(strlen(inp) + 1);
    unsigned i, j;
    for(i = j = 0; i < strlen(inp); i++, j++)
    {
        if(inp[i] != '\\')
        {
            n[j] = inp[i];
            continue;
        }

        i++;
        char gen = 0;
        switch(inp[i])
        {
            case 0:
                die(lineno, "Unexpected end of escaped string: \"%s\"", inp);
            case 't':
                gen = '\t';
                break;
            case 'n':
                gen = '\n';
                break;
            case 'a':
                gen = '\a';
                break;
            case 'b':
                gen = '\b';
                break;
            case 'f':
                gen = '\f';
                break;
            case 'r':
                gen = '\r';
                break;
            case 'v':
                gen = '\v';
                break;
            case '\\':
                gen = '\\';
                break;
            case '\'':
                gen = '\'';
                break;
            default:
                die(lineno, "Unknown escape character in string: \"%s\"", inp);
        }

        n[j] = gen; 
    }

    n[j] = 0;

    return n;
}
