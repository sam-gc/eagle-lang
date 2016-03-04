/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "imports.h"
#include "exports.h"
#include "compiler/ast.h"
#include "grammar/eagle.tab.h"
#include "core/stringbuilder.h"
#include "core/hashtable.h"
#include "core/shipping.h"
#include "core/regex.h"
#include "core/config.h"

#define YY_BUF_SIZE 32768
#define PYES ((void *)(uintptr_t)1)

typedef void* YY_BUFFER_STATE;

extern char *yytext;

extern int skip_type_check;
extern FILE *yyin;
extern int yylex();
extern int yylineno;
extern YY_BUFFER_STATE yy_create_buffer(FILE*, size_t);
extern void yypush_buffer_state(YY_BUFFER_STATE);
extern void yypop_buffer_state();

static hashtable all_imports;
static hashtable imports_exports;
extern char *current_file_name;

char *imp_scan_file(const char *filename)
{
    Strbuilder strb;
    Strbuilder stmp;
    sb_init(&strb);
    sb_init(&stmp);

    FILE *f = fopen(filename, "r");
    YY_BUFFER_STATE buf = yy_create_buffer(f, YY_BUF_SIZE);
    yypush_buffer_state(buf);
    int token;

    export_control *ec = hst_get(&imports_exports, (char *)filename, NULL, NULL);

    int bracket_depth = 0;
    int in_class = 0;
    int save_next = 0;
    char *identifier = NULL;
    int first_time = 1;
    while((token = yylex()) != 0)
    {
        if(token == TTYPEDEF)
        {
            do
            {
                sb_append(&stmp, yytext);
                sb_append(&stmp, " ");
            } while((token = yylex()) != TSEMI);

            sb_append(&stmp, "\n");
        }
        else if(token == TENUM)
        {
            do
            {
                if(token == TSEMI)
                    sb_append(&stmp, "\n");
                sb_append(&stmp, yytext);
                sb_append(&stmp, " ");
            } while((token = yylex()) != TRBRACE);
            sb_append(&stmp, "}\n");
            continue;
        }

        if(token == TLBRACE)
            bracket_depth++;
        if(token == TRBRACE)
        {
            bracket_depth--;
            if(bracket_depth == 0 && in_class)
            {
                in_class = 0;
                sb_append(&stmp, "} ");
            }
        }

        if(token == TCLASS || token == TSTRUCT || token == TINTERFACE)
        {
            in_class = 1;
        }

        if(bracket_depth > (in_class ? 1 : 0) || token == TRBRACE || token == TEXTERN || token == TIMPORT || token == TEXPORT)
            continue;

        if(save_next)
        {
            save_next = 0;
            identifier = strdup(yytext);
        }

        if(token == TCLASS || token == TSTRUCT || token == TINTERFACE || token == TFUNC || token == TGEN)
            save_next = 1;

        if(((token == TFUNC || token == TGEN) && !in_class) || token == TCLASS || token == TSTRUCT)
            sb_append(&stmp, "extern ");

        sb_append(&stmp, yytext);
        sb_append(&stmp, " ");

        if(token == TFUNC || token == TCLASS || token == TSTRUCT || token == TEXTERN || token == TINTERFACE || token == TGEN)
        {
            if(identifier || first_time)
            {
                if(first_time || ec_allow(ec, identifier))
                    sb_append(&strb, stmp.buffer);

                free(stmp.buffer);
                free(identifier);
            }
            first_time = 0;
            sb_init(&stmp);
            identifier = NULL;
        }
    }

     if(identifier || first_time)
     {
         if(first_time || ec_allow(ec, identifier))
             sb_append(&strb, stmp.buffer);

         free(stmp.buffer);
         free(identifier);
     }

    yypop_buffer_state();
    fclose(f);

    return strb.buffer;
}

void imp_build_buffer(void *k, void *v, void *data)
{
    multibuffer *mb = data;
    char *filename = k;

    char *debgname = malloc(100 + strlen(filename));
    sprintf(debgname, "<imported from %s>", filename);

    char *text = imp_scan_file(filename);
//    printf("%s\n", text);
    mb_add_str(mb, text);
    free(text);
}

multibuffer *imp_generate_imports(const char *filename)
{
    all_imports = hst_create();
    imports_exports = hst_create();
    imports_exports.duplicate_keys = 1;

    skip_type_check = 1;

    arraylist work = arr_create(10);
    int offset = 0;
    arr_append(&work, realpath(filename, NULL));

    current_file_name = (char *)"Executable Argument";

    if(!arr_get(&work, 0))
        die(-1, "Unknown code file: %s", filename);

    current_file_name = (char *)filename;

    char *cwd = getcwd(NULL, 0);
    char *current_realpath = realpath(filename, NULL);
    //printf("%s\n", realpath(filename, NULL));

    while(offset < work.count)
    {
        filename = arr_get(&work, offset++);

        export_control *ec = ec_alloc();

        FILE *f = fopen(filename, "r");

        YY_BUFFER_STATE buf = yy_create_buffer(f, YY_BUF_SIZE);
        yypush_buffer_state(buf);

        char *dup = strdup(filename);
        char *dir = dirname(dup);
        chdir(dir);

        int token;
        while((token = yylex()) != 0)
        {
            if(token == TIMPORT)
            {
                char *nw = yytext + 7;
                char *rp = realpath(nw, NULL);

                if(!rp)
                    die(-1, "Imported file (%s) does not exist", nw);

                // Ignore previously viewed files and the current file
                if(IN(all_imports, rp) || !strcmp(rp, current_realpath))
                {
                    free(rp);
                    continue;
                }

                arr_append(&work, rp);
                hst_put(&all_imports, rp, PYES, NULL, NULL);
            }

            if(token == TEXPORT)
                ec_add_wcard(ec, yytext + 7);
        }

        yypop_buffer_state();

        char *rp = realpath(filename, NULL);
        hst_put(&imports_exports, rp, ec, NULL, NULL);
        free(rp);
    }

    // exit(0);

    chdir(cwd);
    free(cwd);
    free(current_realpath);

    multibuffer *buf = mb_alloc();
    hst_for_each(&all_imports, imp_build_buffer, buf);
    skip_type_check = 0;

    hst_free(&imports_exports);

    return buf;
}
