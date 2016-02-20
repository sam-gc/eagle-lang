#define _GNU_SOURCE
#include <clang-c/BuildSystem.h>
#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXErrorCode.h>
#include <clang-c/CXString.h>
#include <clang-c/Documentation.h>
#include <clang-c/Index.h>
#include <clang-c/Platform.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "arraylist.h"

#define CKIND(cursor) clang_getCursorKind(cursor)
#define CTYPE(cursor) clang_getCursorType(cursor)
#define CSTR(cx) clang_getCString(cx)
#define CXSTR(cx) clang_disposeString(cx)
#define CSPELL(cursor) clang_getCursorSpelling(cursor)
#define CT(t) CXType_ ## t

typedef struct {
    CXString  cf_name;
    CXType    cf_ret_type;
    int       cf_n_params;
    arraylist cf_param_types;
    FILE     *output;
} HeaderBundle;

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
    return strdup("");

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
            char *out = strdup(CSTR(spe));
            CXSTR(spe);
            return out;
        }
        default:
            return ch_try_resolve_unknown_type(t);
    }
}

void ch_print_basic_type(HeaderBundle *hb, CXType type)
{
    int pointer_depth = 0;
    if(type.kind == CT(Pointer))
        type = ch_unwrap_pointer(type, &pointer_depth);

    char *res = ch_map_type(&type);
    fputs(res, hb->output);
    free(res);
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

enum CXChildVisitResult
ch_handle_struct_field_cursor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    (void)parent;

    arraylist *list = client_data;

    CXCursor *c = malloc(sizeof(*c));
    memcpy(c, &cursor, sizeof(*c));

    arr_append(list, c);

    return CXChildVisit_Continue;
}

void ch_handle_struct_cursor(CXCursor cursor, HeaderBundle *hb)
{
    arraylist list = arr_create(10);

    clang_visitChildren(cursor, ch_handle_struct_field_cursor, &list);

    if(!list.count)
    {
        arr_free(&list);
        return;
    }

    CXString stname = clang_getTypeSpelling(CTYPE(cursor));
    fprintf(hb->output, "extern %s\n{\n", CSTR(stname));
    CXSTR(stname);

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

enum CXChildVisitResult
ch_ast_dispatch(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    (void)parent; // Silence -pedantic warnings...

    HeaderBundle *hb = (HeaderBundle *)client_data;
    if(CKIND(cursor) == CXCursor_FunctionDecl)
        ch_handle_function_cursor(cursor, hb);
    else if(CKIND(cursor) == CXCursor_StructDecl)
        ch_handle_struct_cursor(cursor, hb);

    return CXChildVisit_Recurse;
}

void ch_generate_symbols_for_file(char *filename, FILE *output)
{
    HeaderBundle hb;
    hb.cf_param_types.count = -1;
    hb.cf_n_params = -5;
    hb.output = output;

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

