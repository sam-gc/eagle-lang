AC_INIT(Eagle, 0.5.0, shorlbec@macalester.edu)

AC_PROG_CC
AC_PROG_CXX
AC_SUBST(stuff, hello)
AC_CHECK_PROG(llvmconf, llvm-config, yes)
AC_PROG_LEX
AC_PROG_YACC

if test $llvmconf == "no"
then
    AC_MSG_ERROR([llvm-config missing])
fi

if ! $LEX --version
then
    AC_MSG_ERROR([lexer missing])
fi

if ! $YACC --version
then
    AC_MSG_ERROR([parser generator missing])
fi

if test `llvm-config --version | awk -F "." '{print $2}'` == "7"
then
    AC_SUBST(llvmvsn, llvm_NEW)
else
    AC_SUBST(llvmvsn, [llvm_OLD])
fi

AC_OUTPUT(Makefile)
AC_OUTPUT(src/core/config.h)
