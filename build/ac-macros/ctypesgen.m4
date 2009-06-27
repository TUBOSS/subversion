dnl
dnl ctypesgen.m4: Locates ctypesgen for building/installing ctypes-python.
dnl

AC_DEFUN(SVN_CHECK_CTYPESGEN,
[
  AC_ARG_WITH(ctypesgen,
              AS_HELP_STRING([--with-ctypesgen=PATH],
                             [Specify the path to ctypesgen.  This can either
                              be the full path to a ctypesgen installation,
                              the full path to a ctypesgen source tree or the
                              full path to ctypesgen.py.]),
  [
    case "$withval" in
      "no")
        SVN_FIND_CTYPESGEN(no)
      ;;
      "yes")
        SVN_FIND_CTYPESGEN(check)
      ;;
      *)
        SVN_FIND_CTYPESGEN($withval)
      ;;
    esac
  ],
  [
    SVN_FIND_CTYPESGEN(check)
  ])
])

AC_DEFUN(SVN_FIND_CTYPESGEN,
[
  where=$1

  CTYPESGEN=none

  if test $where = check; then
    AC_PATH_PROG(CTYPESGEN, "ctypesgen.py", none)
  elif test $where != no; then
    AC_MSG_CHECKING([for ctypesgen.py])

    if test -f "$where"; then
      CTYPESGEN="$where"
    elif test -f "$where/bin/ctypesgen.py"; then
      CTYPESGEN="$where/bin/ctypesgen.py"
    else
      CTYPESGEN="$where/ctypesgen.py"
    fi

    if test ! -f "$CTYPESGEN" || test ! -x "$CTYPESGEN"; then
      SVN_MSG_ERROR([Could not find ctypesgen at $where/ctypesgen.py or at
                     $where/bin/ctypesgen.py])
    else
      AC_MSG_RESULT([$CTYPESGEN])
    fi
  fi

  dnl We use CTYPESGEN in the Makefile
  AC_SUBST(CTYPESGEN)
])
