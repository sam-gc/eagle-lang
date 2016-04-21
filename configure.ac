AC_INIT(Eagle, 0.6.0, shorlbec@macalester.edu)

AC_PROG_CC
AC_PROG_CXX
AC_SUBST(stuff, hello)
AC_PROG_LEX
AC_PROG_YACC
AC_CHECK_PROGS([llvmconfig], [llvm-config llvm-config-3.5 llvm-config-3.6 llvm-config-3.7], no, $PATH)
AC_CHECK_PROG(PYTHON, python, "yes")

AC_CHECK_HEADER(pthread.h, have_pthread="yes")

if test "$have_pthread" == "yes"
then
    AC_MSG_NOTICE([Using pthreads])
    AC_SUBST(haspthreads, "HAS_PTHREAD")
else
    AC_MSG_NOTICE([No pthread support])
    AC_SUBST(haspthreads, "NO_PTHREAD")
fi

AC_ARG_ENABLE(debug, [--enable-debug        enable debug mode])
AC_ARG_WITH(llvm-config, [--with-llvm-config          set the llvm-config program], llvmconfprog=$withval, llvmconfprog="no")

if test "$enable_debug" == "yes"
then
    AC_MSG_NOTICE([Debug mode is enabled])
    AC_SUBST(targ, "-O0 -g")
    AC_SUBST(debug, "DEBUG")
else
    AC_SUBST(targ, -O3)
    AC_SUBST(debug, "RELEASE")
fi

if test $llvmconfig == "no"
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

if test $PYTHON == "no"
then
     AC_MSG_ERROR([python missing])
fi

python linequoter.py > rc.egl.out
AC_SUBST(configcode, `cat rc.egl.out`)

rm -rf rc.egl.out

AC_OUTPUT(Makefile)
AC_OUTPUT(src/core/config.h)
AC_OUTPUT(src/core/config.c)
