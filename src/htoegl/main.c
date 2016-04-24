/*
 * Copyright (c) 2015-2016 Sam Horlbeck Olsen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define _GNU_SOURCE
#include <clang-c/Index.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "arraylist.h"
#include "hashtable.h"

#define CKIND(cursor) clang_getCursorKind(cursor)
#define CTYPE(cursor) clang_getCursorType(cursor)
#define CSTR(cx) clang_getCString(cx)
#define CXSTR(cx) clang_disposeString(cx)
#define CSPELL(cursor) clang_getCursorSpelling(cursor)
#define CT(t) CXType_ ## t
#define CC(t) CXCursor_ ## t

#define ONE_PTR ((void *)(uintptr_t)1)

typedef struct {
    CXString  cf_name;
    CXType    cf_ret_type;
    int       cf_n_params;
    Arraylist cf_param_types;
    FILE     *output;
    Hashtable seen;
} HeaderBundle;

void ch_print_basic_type(HeaderBundle *hb, CXType type);

CXType ch_unwrap_pointer(CXType pointer, int *pointer_depth)
{
    while(pointer.kind == CXType_Pointer)
    {
        (*pointer_depth)++;
        pointer = clang_getPointeeType(pointer);
    }

    return pointer;
}

char *ch_try_resolve_unknown_type(CXType *t)
{
    CXString spelling = clang_getTypeSpelling(*t);
    const char *bytes = CSTR(spelling);
    char buffer[10];
    size_t len = strlen(bytes);
    char *output;

    if(t->kind != CXType_Unexposed)
        goto error;

    if(len < 4)
        goto error;

    memcpy(buffer, bytes, 4);
    buffer[4] = '\0';

    if(strcmp(buffer, "enum") == 0)
    {
        output = strdup(bytes + 5);
        goto success;
    }

    if(len < 5)
        goto error;

    memcpy(buffer, bytes, 5);
    buffer[5] = '\0';

    if(strcmp(buffer, "union") == 0)
    {
        output = malloc(len + 10);
        sprintf(output, "[union %s]", bytes + 6);
        goto success;
    }

    if(len < 6)
        goto error;

    memcpy(buffer, bytes, 6);
    buffer[6] = '\0';

    if(strcmp(buffer, "struct") == 0)
    {
        output = strdup(bytes + 7);
        goto success;
    }
error:
    fprintf(stderr, "Unknown: %s (%d)\n", CSTR(spelling), t->kind);
    CXSTR(spelling);
    return strdup("INVALID_UNKNOWN_TYPE");

success:
    CXSTR(spelling);
    return output;
}

// Caller frees
char *ch_map_type(CXType *t)
{
    switch(t->kind)
    {
        case CT(Int):
        case CT(UInt):
            return strdup("int");
        case CT(Short):
        case CT(UShort):
            return strdup("short");
        case CT(Void):  return strdup("any");
        case CT(Bool):  return strdup("bool");
        case CT(Long):
        case CT(LongLong):
        case CT(ULong):
        case CT(ULongLong):
            return strdup("long");
        case CT(Char_S):
        case CT(Char_U):
        case CT(SChar):
        case CT(UChar):
            return strdup("byte");
        case CT(Float):
        case CT(Double):
        case CT(LongDouble):
            return strdup("double");
        case CT(Typedef):
        {
            CXString spe = clang_getTypeSpelling(*t);
            int offset = clang_isConstQualifiedType(*t) ? 6 : 0;
            char *out = strdup(CSTR(spe) + offset);
            CXSTR(spe);
            return out;
        }
        default:
            return ch_try_resolve_unknown_type(t);
    }
}

void ch_print_fptr_type(HeaderBundle *hb, CXType type)
{
    fputs("[", hb->output);

    int nparams = clang_getNumArgTypes(type);
    for(int i = 0; i < nparams; i++)
    {
        CXType p = clang_getArgType(type, i);
        ch_print_basic_type(hb, p);
        if(i < nparams - 1) fputs(", ", hb->output);
    }

    CXType rett = clang_getResultType(type);
    fputs(" : ", hb->output);
    if(rett.kind != CT(Void))
        ch_print_basic_type(hb, rett);

    fputs("]*", hb->output);
}

void ch_print_basic_type(HeaderBundle *hb, CXType type)
{
    int pointer_depth = 0;
    if(type.kind == CT(Pointer))
        type = ch_unwrap_pointer(type, &pointer_depth);

    if(type.kind == CT(ConstantArray))
    {
        CXType at = clang_getArrayElementType(type);
        int sz = clang_getArraySize(type);

        ch_print_basic_type(hb, at);
        fprintf(hb->output, "[%d]", sz);
    }
    else if(type.kind == CT(FunctionProto))
    {
        ch_print_fptr_type(hb, type);
    }
    else
    {
        char *res = ch_map_type(&type);
        fputs(res, hb->output);
        free(res);
    }

    for(int i = 0; i < pointer_depth; i++)
    {
        fputs("*", hb->output);
    }
}

void ch_handle_function_cursor(CXCursor cursor, HeaderBundle *hb)
{
    CXType func = CTYPE(cursor);
    int nparams = clang_getNumArgTypes(func);
    CXString name = CSPELL(cursor);
    CXType rett = clang_getResultType(func);

    fprintf(hb->output, "extern func %s(", CSTR(name));
    CXSTR(name);

    for(int i = 0; i < nparams; i++)
    {
        CXType p = clang_getArgType(func, i);
        ch_print_basic_type(hb, p);
        if(i < nparams - 1) fputs(", ", hb->output);
    }

    fputs(")", hb->output);

    if(rett.kind != CT(Void))
    {
        fputs(" : ", hb->output);
        ch_print_basic_type(hb, rett);
    }

    fputs("\n", hb->output);
}

char *ch_add_info_where_necessary(const char *info, const char *name)
{
    size_t infolen = strlen(info);
    size_t namelen = strlen(name);
    char buffer[infolen + 1];

    if(infolen > namelen)
    {
        char *output = malloc(namelen + infolen + 5);
        sprintf(output, "%s %s", info, name);
        return output;
    }

    memcpy(buffer, name, infolen);
    buffer[infolen] = '\0';
    if(strcmp(buffer, info) == 0)
    {
        return strdup(name);
    }

    char *output = malloc(namelen + infolen + 5);
    sprintf(output, "%s %s", info, name);
    return output;
}

enum CXChildVisitResult
ch_handle_struct_field_cursor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    (void)parent;

    Arraylist *list = client_data;

    CXCursor *c = malloc(sizeof(*c));
    memcpy(c, &cursor, sizeof(*c));

    arr_append(list, c);

    return CXChildVisit_Continue;
}

void ch_handle_struct_cursor(CXCursor cursor, HeaderBundle *hb)
{
    Arraylist list = arr_create(10);
    clang_visitChildren(cursor, ch_handle_struct_field_cursor, &list);

    if(!list.count)
    {
        arr_free(&list);
        return;
    }

    CXString stname = clang_getTypeSpelling(CTYPE(cursor));

    if(hst_get(&hb->seen, (char *)CSTR(stname), NULL, NULL))
    {
        arr_free(&list);
        CXSTR(stname);
        return;
    }

    hst_put(&hb->seen, (char *)CSTR(stname), ONE_PTR, NULL, NULL);

    char *text = ch_add_info_where_necessary("struct", CSTR(stname));
    fprintf(hb->output, "extern %s\n{\n", text);
    CXSTR(stname);
    free(text);

    for(int i = 0; i < list.count; i++)
    {
        cursor = *((CXCursor *)list.items[i]);
        fputs("    ", hb->output);
        ch_print_basic_type(hb, CTYPE(cursor));

        CXString name = CSPELL(cursor);
        fprintf(hb->output, " %s\n", CSTR(name));
        CXSTR(name);

        free(list.items[i]);
    }

    arr_free(&list);

    fputs("}\n", hb->output);
}

static long enum_value = 0;
enum CXChildVisitResult
ch_handle_enum_constant_cursor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    HeaderBundle *hb = (HeaderBundle *)client_data;
    (void)parent;

    if(CKIND(cursor) != CC(EnumConstantDecl))
    {
        return CXChildVisit_Recurse;
    }

    CXString name = CSPELL(cursor);
    fprintf(hb->output, "    %s", CSTR(name));
    CXSTR(name);

    long val = (long)clang_getEnumConstantDeclValue(cursor);
    if(val != enum_value)
    {
        enum_value = val;
        fprintf(hb->output, " : %ld\n", enum_value);
    }
    else
        fputs("\n", hb->output);


    enum_value++;
    return CXChildVisit_Recurse;
}

void ch_handle_enum_cursor(CXCursor cursor, HeaderBundle *hb)
{
    CXString ename = clang_getTypeSpelling(CTYPE(cursor));
    char *text = ch_add_info_where_necessary("enum", CSTR(ename));
    if(hst_get(&hb->seen, (char *)CSTR(ename), NULL, NULL))
    {
        CXSTR(ename);
        return;
    }

    hst_put(&hb->seen, (char *)CSTR(ename), ONE_PTR, NULL, NULL);
    CXSTR(ename);

    fprintf(hb->output, "%s\n{\n", text);
    free(text);

    enum_value = 0;
    clang_visitChildren(cursor, ch_handle_enum_constant_cursor, hb);

    fputs("}\n", hb->output);
}

void ch_handle_typedef_cursor(CXCursor cursor, HeaderBundle *hb)
{
    CXString newname = CSPELL(cursor);

    fputs("typedef ", hb->output);
    ch_print_basic_type(hb, clang_getTypedefDeclUnderlyingType(cursor));
    fprintf(hb->output, " %s\n", CSTR(newname));

    CXSTR(newname);
}

enum CXChildVisitResult
ch_ast_dispatch(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    (void)parent; // Silence -pedantic warnings...

    HeaderBundle *hb = (HeaderBundle *)client_data;

    switch(CKIND(cursor))
    {
        case CC(FunctionDecl):
            ch_handle_function_cursor(cursor, hb);
            break;
        case CC(StructDecl):
            ch_handle_struct_cursor(cursor, hb);
            break;
        case CC(TypedefDecl):
            ch_handle_typedef_cursor(cursor, hb);
            break;
        case CC(EnumDecl):
            ch_handle_enum_cursor(cursor, hb);
            break;
        default:
            break;
    }

    return CXChildVisit_Recurse;
}

void ch_generate_symbols_for_file(char *filename, FILE *output)
{
    HeaderBundle hb;
    hb.cf_param_types.count = -1;
    hb.cf_n_params = -5;
    hb.output = output;
    hb.seen = hst_create();
    hb.seen.duplicate_keys = 1;

    CXIndex index = clang_createIndex(0, 0);
    const char *args[] = {
        "/usr/lib/gcc/x86_64-unknown-linux-gnu/5.3.0/include",
        "/usr/local/include",
        "/usr/lib/gcc/x86_64-unknown-linux-gnu/5.3.0/include-fixed",
        "-I/usr/include",
        "-I."
    };

    CXTranslationUnit tu = clang_parseTranslationUnit(index, filename, args, 5, NULL, 0,
            CXTranslationUnit_None);

    clang_visitChildren(clang_getTranslationUnitCursor(tu), ch_ast_dispatch, &hb);

    hst_free(&hb.seen);
}

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage: %s <header.h> [output.egl]\n", argv[0]);
        return 0;
    }

    FILE *output = argc == 3 ? fopen(argv[2], "w") : stdout;
    if(!output)
    {
        fprintf(stderr, "Failed to open output file: %s\n", argv[2]);
        return 0;
    }
    ch_generate_symbols_for_file(argv[1], output);

    fclose(output);

    return 0;
}
