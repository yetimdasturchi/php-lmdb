PHP_ARG_WITH([lmdb-php],
  [for lmdb-php support],
  [AS_HELP_STRING([--with-lmdb-php], [Include lmdb-php support])])

if test "$PHP_LMDB_PHP" != "no"; then
  PHP_CHECK_LIBRARY([lmdb], [mdb_env_create], [
    PHP_ADD_LIBRARY([lmdb], 1, LMDB_PHP_SHARED_LIBADD)
  ], [
    AC_MSG_ERROR([liblmdb not found])
  ])

  PHP_NEW_EXTENSION([lmdb_php], [lmdb-php_wrap.c], [$ext_shared])
  PHP_SUBST([LMDB_PHP_SHARED_LIBADD])
fi
