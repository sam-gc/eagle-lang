#include <clang-c/BuildSystem.h>
#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXErrorCode.h>
#include <clang-c/CXString.h>
#include <clang-c/Documentation.h>
#include <clang-c/Index.h>
#include <clang-c/Platform.h>
#include <stdio.h>
#include "c-headers.h"
#include "arraylist.h"

#define CKIND(cursor) clang_getCursorKind(cursor)
#define CTYPE(cursor) clang_getCursorType(cursor)
#define CSTR(cx) clang_getCString(cx)
#define CXSTR(cx) clang_disposeString(cx)
#define CSPELL(cursor) clang_getCursorSpelling(cursor)

typedef struct {
    hashtable *sym_t;
    CXString  cf_name;
    CXType    cf_ret_type;
    int       cf_n_params;
    arraylist cf_param_types;
} HeaderBundle;

enum CXChildVisitResult
ch_ast_dispatch(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    HeaderBundle *hb = (HeaderBundle *)client_data;
    if(CKIND(cursor) == CXCursor_FunctionDecl)
    {
        CXType func = CTYPE(cursor);
        int nparams = clang_getNumArgTypes(func);
        CXString name = CSPELL(cursor);
        CXType rett = clang_getResultType(func);

    }
}

hashtable ch_generate_symbols_for_file(char *filename)
{
    hashtable symbols = hst_create();
    HeaderBundle hb;
    hb.sym_t = &symbols;
    hb.cf_param_types.count = -1;
    hb.cf_n_params = -5;

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

    return symbols;
}

#ifdef HTEST

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage: %s <file>\n", argv[0]);
        return 0;
    }

    ch_generate_symbols_for_file(argv[1]);

    return 0;
}

#endif

