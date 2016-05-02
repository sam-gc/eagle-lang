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
#define IS_ID_AND_EQ(tok, text, targ) ((tok) == TIDENTIFIER && strcmp((text), (targ)) == 0)

typedef void* YY_BUFFER_STATE;

extern char *yytext;

extern int skip_type_check;
extern FILE *yyin;
extern int yylex();
extern int yylineno;
extern YY_BUFFER_STATE yy_create_buffer(FILE*, size_t);
extern void yypush_buffer_state(YY_BUFFER_STATE);
extern void yypop_buffer_state();

static Hashtable all_imports;
static Hashtable imports_exports;
extern char *current_file_name;

typedef struct {
    char *full_text;
    char *symbol;
} ImportUnit;

static void imp_iufree(ImportUnit *iu)
{
    free(iu->full_text);
    free(iu->symbol);
}

static void imp_consume_body(Strbuilder *string)
{
    int bracket_depth = 1;
    int token;
    while(bracket_depth)
    {
        token = yylex();
        if(token == TLBRACE)
            bracket_depth++;
        else if(token == TRBRACE)
            bracket_depth--;

        if(string)
        {
            sb_append(string, yytext);
            sb_append(string, " ");
        }
    }
}

static ImportUnit imp_parse_function(int inclass, char *intext, int extdecl)
{
    ImportUnit iu = {NULL, NULL};

    Strbuilder string;
    sb_init(&string);

    if(!inclass)
        sb_append(&string, "extern ");

    sb_append(&string, intext);
    sb_append(&string, " ");

    int token = yylex();

    /*
    if(token != TIDENTIFIER)
        die(yylineno, "%s name not properly defined", intext);
    */

    iu.symbol = strdup(yytext);

    int terminal = extdecl ? TSEMI : TLBRACE;

    do 
    {
        sb_append(&string, yytext);
        sb_append(&string, " ");
    }
    while((token = yylex()) != terminal);

    if(!extdecl)
        imp_consume_body(NULL);

    iu.full_text = string.buffer;

    return iu;
}

static ImportUnit imp_parse_typedef()
{
    ImportUnit iu = {NULL, NULL};

    Strbuilder string;
    sb_init(&string);

    sb_append(&string, "typedef ");

    char *last = NULL;

    int token;
    while((token = yylex()) != TSEMI)
    {
        sb_append(&string, yytext);
        sb_append(&string, " ");
        if(last)
            free(last);
        last = strdup(yytext);
    }

    iu.full_text = string.buffer;
    iu.symbol = last;

    return iu;
}

static ImportUnit imp_parse_bracketed(int declext, const char *what)
{
    ImportUnit iu = {NULL, NULL};

    Strbuilder string;
    sb_init(&string);

    if(declext)
        sb_append(&string, "extern ");

    sb_append(&string, what);
    sb_append(&string, " ");

    int token = yylex();
    if(token != TIDENTIFIER)
        die(yylineno, "%s name not properly defined", what);

    iu.symbol = strdup(yytext);

    sb_append(&string, yytext);
    sb_append(&string, " ");

    while((token = yylex()) != TRBRACE)
    {
        sb_append(&string, yytext);
        sb_append(&string, " ");
    }

    sb_append(&string, "}");

    iu.full_text = string.buffer;

    return iu;
}

static ImportUnit imp_parse_interface()
{
    return imp_parse_bracketed(0, "interface");
}

static ImportUnit imp_parse_enum()
{
    return imp_parse_bracketed(0, "enum");
}

static ImportUnit imp_parse_struct()
{
    return imp_parse_bracketed(1, "struct");
}

static ImportUnit imp_parse_class(int extdecl)
{
    ImportUnit iu = {NULL, NULL};

    Strbuilder string;
    sb_init(&string);

    sb_append(&string, "extern class ");

    int token = yylex();
    if(token != TIDENTIFIER)
        die(yylineno, "class name not properly defined");

    iu.symbol = strdup(yytext);

    do
    {
        sb_append(&string, yytext);
        sb_append(&string, " ");
    }
    while((token = yylex()) != TLBRACE);

    sb_append(&string, "\n{\n");
    if(extdecl)
    {
        imp_consume_body(&string);
        iu.full_text = string.buffer;
        return iu;
    }

    while((token = yylex()) != TRBRACE)
    {
        if(token == TFUNC || token == TVIEW ||
           IS_ID_AND_EQ(token, yytext, "init") ||
           IS_ID_AND_EQ(token, yytext, "destruct"))
        {
            ImportUnit u = imp_parse_function(1, yytext, 0);
            sb_append(&string, u.full_text);

            imp_iufree(&u);
            continue;
        }

        sb_append(&string, yytext);
        sb_append(&string, " ");
    }

    sb_append(&string, "\n}\n");

    iu.full_text = string.buffer;

    return iu;
}

static char *imp_scan_file(const char *filename)
{
    Strbuilder string;
    sb_init(&string);

    FILE *f = fopen(filename, "r");
    YY_BUFFER_STATE buf = yy_create_buffer(f, YY_BUF_SIZE);
    yypush_buffer_state(buf);
    int token, is_extern = 0, save_next = 0;
    ImportUnit iu;

    ExportControl *ec = hst_get(&imports_exports, (char *)filename, NULL, NULL);

    while((token = yylex()) != 0)
    {
        switch(token)
        {
            case TEXTERN:
                is_extern = 1;
            case TIMPORT:
            case TSEMI:
            case TCSTR:
            case TRPAREN:
                save_next = 0;
                continue;

            case TLPAREN:
                yylex();
                continue;

            case TEXPORT:
                save_next = 1;
                continue;

            case TFUNC:
            case TGEN:
                iu = imp_parse_function(0, yytext, is_extern);
                break;
            case TSTRUCT:
                iu = imp_parse_struct();
                break;
            case TCLASS:
                iu = imp_parse_class(is_extern);
                break;
            case TTYPEDEF:
                iu = imp_parse_typedef();
                break;
            case TENUM:
                iu = imp_parse_enum();
                break;
            case TINTERFACE:
                iu = imp_parse_interface();
                break;
            default:
                die(yylineno, "Unknown top-level symbol");
        }

        is_extern = 0;
        if(ec_allow(ec, iu.symbol, token) || save_next)
        {
            save_next = 0;
            sb_append(&string, iu.full_text);
            sb_append(&string, "\n");
        }

        imp_iufree(&iu);
    }

    yypop_buffer_state();

    return string.buffer;
}

void imp_build_buffer(void *k, void *v, void *data)
{
    Multibuffer *mb = data;
    char *filename = k;

    char *debgname = malloc(100 + strlen(filename));
    sprintf(debgname, "<imported from %s>", filename);

    char *text = imp_scan_file(filename);
//    printf("%s\n", text);
    mb_add_str(mb, text);
    free(text);
}

Multibuffer *imp_generate_imports(const char *filename)
{
    all_imports = hst_create();
    imports_exports = hst_create();
    imports_exports.duplicate_keys = 1;

    skip_type_check = 1;

    Arraylist work = arr_create(10);
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

        ExportControl *ec = ec_alloc();

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

            if(token == TEXPORT && (((token = yylex()) == TCSTR) || token == TLPAREN))
            {
                int tok = 0;
                if(token == TLPAREN)
                {
                    tok = yylex();
                    yylex();
                    token = yylex();
                }

                char buf[strlen(yytext) - 2];
                memcpy(buf, yytext + 1, strlen(yytext) - 2);
                buf[strlen(yytext) - 2] = '\0';
                ec_add_wcard(ec, buf, tok);
            }
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

    Multibuffer *buf = mb_alloc();
    hst_for_each(&all_imports, imp_build_buffer, buf);
    skip_type_check = 0;

    hst_free(&imports_exports);

    return buf;
}
