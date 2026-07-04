#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_lmdb_php.h"
#include "lmdb.h"

#include <stdint.h>
#include <string.h>

static int le_lmdb_env;
static int le_lmdb_txn;
static int le_lmdb_dbi;
static int le_lmdb_cursor;
static int le_lmdb_val;
static int le_lmdb_stat;
static int le_lmdb_envinfo;

typedef struct {
	MDB_dbi dbi;
} php_lmdb_dbi;

typedef struct {
	MDB_val val;
	zend_string *storage;
} php_lmdb_val;

static void php_lmdb_val_dtor(zend_resource *rsrc)
{
	php_lmdb_val *value = (php_lmdb_val *) rsrc->ptr;
	if (value) {
		if (value->storage) {
			zend_string_release(value->storage);
		}
		efree(value);
	}
}

static void php_lmdb_stat_dtor(zend_resource *rsrc)
{
	efree(rsrc->ptr);
}

static void php_lmdb_envinfo_dtor(zend_resource *rsrc)
{
	efree(rsrc->ptr);
}

static void php_lmdb_dbi_dtor(zend_resource *rsrc)
{
	efree(rsrc->ptr);
}

static MDB_env *php_lmdb_env_from_zval(zval *zv)
{
	return (MDB_env *) zend_fetch_resource_ex(zv, "MDB_env", le_lmdb_env);
}

static MDB_txn *php_lmdb_txn_from_zval(zval *zv)
{
	return (MDB_txn *) zend_fetch_resource_ex(zv, "MDB_txn", le_lmdb_txn);
}

static MDB_cursor *php_lmdb_cursor_from_zval(zval *zv)
{
	return (MDB_cursor *) zend_fetch_resource_ex(zv, "MDB_cursor", le_lmdb_cursor);
}

static MDB_val *php_lmdb_val_from_zval(zval *zv)
{
	php_lmdb_val *value = (php_lmdb_val *) zend_fetch_resource_ex(zv, "MDB_val", le_lmdb_val);
	return value ? &value->val : NULL;
}

static MDB_dbi php_lmdb_dbi_from_zval(zval *zv)
{
	php_lmdb_dbi *dbi = (php_lmdb_dbi *) zend_fetch_resource_ex(zv, "MDB_dbi", le_lmdb_dbi);
	return dbi ? dbi->dbi : 0;
}

static void php_lmdb_update_val_storage(MDB_val *val, const char *data, size_t len)
{
	php_lmdb_val *owner = (php_lmdb_val *) ((char *) val - XtOffsetOf(php_lmdb_val, val));

	if (owner->storage) {
		zend_string_release(owner->storage);
	}

	owner->storage = zend_string_init(data, len, 0);
	owner->val.mv_size = len;
	owner->val.mv_data = ZSTR_VAL(owner->storage);
}

PHP_FUNCTION(mdb_version)
{
	zval *major_zv = NULL, *minor_zv = NULL, *patch_zv = NULL;
	int major = 0, minor = 0, patch = 0;
	char *version;

	ZEND_PARSE_PARAMETERS_START(0, 3)
		Z_PARAM_OPTIONAL
		Z_PARAM_ZVAL(major_zv)
		Z_PARAM_ZVAL(minor_zv)
		Z_PARAM_ZVAL(patch_zv)
	ZEND_PARSE_PARAMETERS_END();

	version = mdb_version(&major, &minor, &patch);
	if (major_zv) {
		zval_dtor(major_zv);
		ZVAL_LONG(major_zv, major);
	}
	if (minor_zv) {
		zval_dtor(minor_zv);
		ZVAL_LONG(minor_zv, minor);
	}
	if (patch_zv) {
		zval_dtor(patch_zv);
		ZVAL_LONG(patch_zv, patch);
	}

	RETURN_STRING(version);
}

PHP_FUNCTION(mdb_strerror)
{
	zend_long err;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(err)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_STRING(mdb_strerror((int) err));
}

PHP_FUNCTION(mdb_env_create)
{
	MDB_env *env = NULL;
	int rc;

	ZEND_PARSE_PARAMETERS_NONE();

	rc = mdb_env_create(&env);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_env_create: %d", rc);
		RETURN_NULL();
	}

	RETURN_RES(zend_register_resource(env, le_lmdb_env));
}

PHP_FUNCTION(mdb_env_open)
{
	zval *env_zv;
	char *path;
	size_t path_len;
	zend_long flags, mode;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_STRING(path, path_len)
		Z_PARAM_LONG(flags)
		Z_PARAM_LONG(mode)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_open(env, path, (unsigned int) flags, (mdb_mode_t) mode));
}

PHP_FUNCTION(mdb_env_copy)
{
	zval *env_zv;
	char *path;
	size_t path_len;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_STRING(path, path_len)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_copy(env, path));
}

PHP_FUNCTION(mdb_env_copyfd)
{
	zval *env_zv;
	zend_long fd;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(fd)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_copyfd(env, (mdb_filehandle_t) fd));
}

PHP_FUNCTION(mdb_env_copy2)
{
	zval *env_zv;
	char *path;
	size_t path_len;
	zend_long flags;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_STRING(path, path_len)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_copy2(env, path, (unsigned int) flags));
}

PHP_FUNCTION(mdb_env_copyfd2)
{
	zval *env_zv;
	zend_long fd, flags;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(fd)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_copyfd2(env, (mdb_filehandle_t) fd, (unsigned int) flags));
}

PHP_FUNCTION(mdb_env_stat)
{
	zval *env_zv;
	MDB_env *env;
	MDB_stat *stat;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	stat = emalloc(sizeof(MDB_stat));
	rc = mdb_env_stat(env, stat);
	if (rc != MDB_SUCCESS) {
		efree(stat);
		php_error_docref(NULL, E_NOTICE, "mdb_env_stat: %d", rc);
		RETURN_NULL();
	}

	RETURN_RES(zend_register_resource(stat, le_lmdb_stat));
}

PHP_FUNCTION(mdb_env_info)
{
	zval *env_zv;
	MDB_env *env;
	MDB_envinfo *info;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	info = emalloc(sizeof(MDB_envinfo));
	rc = mdb_env_info(env, info);
	if (rc != MDB_SUCCESS) {
		efree(info);
		php_error_docref(NULL, E_NOTICE, "mdb_env_info: %d", rc);
		RETURN_NULL();
	}

	RETURN_RES(zend_register_resource(info, le_lmdb_envinfo));
}

PHP_FUNCTION(mdb_env_sync)
{
	zval *env_zv;
	zend_long force;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(force)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_sync(env, (int) force));
}

PHP_FUNCTION(mdb_env_close)
{
	zval *env_zv;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	mdb_env_close(env);
	RETURN_NULL();
}

PHP_FUNCTION(mdb_env_set_flags)
{
	zval *env_zv;
	zend_long flags, onoff;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(flags)
		Z_PARAM_LONG(onoff)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_set_flags(env, (unsigned int) flags, (int) onoff));
}

PHP_FUNCTION(mdb_env_get_flags)
{
	zval *env_zv;
	MDB_env *env;
	unsigned int flags = 0;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	rc = mdb_env_get_flags(env, &flags);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_env_get_flags: %d", rc);
		RETURN_LONG(-1);
	}

	RETURN_LONG(flags);
}

PHP_FUNCTION(mdb_env_get_path)
{
	zval *env_zv;
	MDB_env *env;
	const char *path = NULL;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	rc = mdb_env_get_path(env, &path);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_env_get_path: %d", rc);
		RETURN_NULL();
	}

	RETURN_STRING(path ? path : "");
}

PHP_FUNCTION(mdb_env_get_fd)
{
	zval *env_zv;
	MDB_env *env;
	mdb_filehandle_t fd = 0;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	rc = mdb_env_get_fd(env, &fd);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_env_get_fd: %d", rc);
		RETURN_LONG(-1);
	}

	RETURN_LONG((zend_long) fd);
}

PHP_FUNCTION(mdb_env_set_mapsize)
{
	zval *env_zv;
	zend_long size;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(size)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_set_mapsize(env, (size_t) size));
}

PHP_FUNCTION(mdb_env_set_maxreaders)
{
	zval *env_zv;
	zend_long readers;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(readers)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_set_maxreaders(env, (unsigned int) readers));
}

PHP_FUNCTION(mdb_env_get_maxreaders)
{
	zval *env_zv;
	MDB_env *env;
	unsigned int readers = 0;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	rc = mdb_env_get_maxreaders(env, &readers);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_env_get_maxreaders: %d", rc);
		RETURN_LONG(-1);
	}

	RETURN_LONG(readers);
}

PHP_FUNCTION(mdb_env_set_maxdbs)
{
	zval *env_zv;
	zend_long dbs;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_LONG(dbs)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_set_maxdbs(env, (MDB_dbi) dbs));
}

PHP_FUNCTION(mdb_env_get_maxkeysize)
{
	zval *env_zv;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_env_get_maxkeysize(env));
}

PHP_FUNCTION(mdb_env_set_userctx)
{
	zval *env_zv;
	char *value;
	size_t value_len;
	MDB_env *env;
	char *copy;
	int rc;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_STRING(value, value_len)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	copy = estrndup(value, value_len);
	rc = mdb_env_set_userctx(env, copy);
	if (rc != MDB_SUCCESS) {
		efree(copy);
		RETURN_LONG(rc);
	}

	RETURN_LONG(rc);
}

PHP_FUNCTION(mdb_env_get_userctx)
{
	zval *env_zv;
	MDB_env *env;
	void *ctx;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	ctx = mdb_env_get_userctx(env);
	if (!ctx) {
		RETURN_NULL();
	}

	RETURN_STRING((char *) ctx);
}

PHP_FUNCTION(mdb_txn_begin)
{
	zval *env_zv, *parent_zv;
	zend_long flags;
	MDB_env *env;
	MDB_txn *parent = NULL;
	MDB_txn *txn = NULL;
	int rc;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_ZVAL(parent_zv)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	if (Z_TYPE_P(parent_zv) != IS_NULL) {
		parent = php_lmdb_txn_from_zval(parent_zv);
		if (!parent) {
			RETURN_THROWS();
		}
	}

	rc = mdb_txn_begin(env, parent, (unsigned int) flags, &txn);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_txn_begin: %d", rc);
		RETURN_NULL();
	}

	RETURN_RES(zend_register_resource(txn, le_lmdb_txn));
}

PHP_FUNCTION(mdb_txn_env)
{
	zval *txn_zv;
	MDB_txn *txn;
	MDB_env *env;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(txn_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	env = mdb_txn_env(txn);
	RETURN_RES(zend_register_resource(env, le_lmdb_env));
}

PHP_FUNCTION(mdb_txn_id)
{
	zval *txn_zv;
	MDB_txn *txn;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(txn_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) mdb_txn_id(txn));
}

PHP_FUNCTION(mdb_txn_commit)
{
	zval *txn_zv;
	MDB_txn *txn;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(txn_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_txn_commit(txn));
}

PHP_FUNCTION(mdb_txn_abort)
{
	zval *txn_zv;
	MDB_txn *txn;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(txn_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	mdb_txn_abort(txn);
	RETURN_NULL();
}

PHP_FUNCTION(mdb_txn_reset)
{
	zval *txn_zv;
	MDB_txn *txn;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(txn_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	mdb_txn_reset(txn);
	RETURN_NULL();
}

PHP_FUNCTION(mdb_txn_renew)
{
	zval *txn_zv;
	MDB_txn *txn;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(txn_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_txn_renew(txn));
}

PHP_FUNCTION(mdb_dbi_open)
{
	zval *txn_zv, *name_zv;
	zend_long flags;
	MDB_txn *txn;
	const char *name = NULL;
	MDB_dbi dbi;
	php_lmdb_dbi *dbi_res;
	int rc;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_ZVAL(name_zv)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	if (Z_TYPE_P(name_zv) != IS_NULL) {
		name = Z_STRVAL_P(name_zv);
	}

	rc = mdb_dbi_open(txn, name, (unsigned int) flags, &dbi);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_dbi_open: %d", rc);
		RETURN_NULL();
	}

	dbi_res = emalloc(sizeof(php_lmdb_dbi));
	dbi_res->dbi = dbi;
	RETURN_RES(zend_register_resource(dbi_res, le_lmdb_dbi));
}

PHP_FUNCTION(mdb_dbi_close)
{
	zval *env_zv, *dbi_zv;
	MDB_env *env;
	MDB_dbi dbi;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(env_zv)
		Z_PARAM_RESOURCE(dbi_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	mdb_dbi_close(env, dbi);
	RETURN_NULL();
}

PHP_FUNCTION(mdb_drop)
{
	zval *txn_zv, *dbi_zv;
	zend_long del;
	MDB_txn *txn;
	MDB_dbi dbi;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_LONG(del)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	RETURN_LONG(mdb_drop(txn, dbi, (int) del));
}

PHP_FUNCTION(mdb_stat)
{
	zval *txn_zv, *dbi_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_stat *stat;
	int rc;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	stat = emalloc(sizeof(MDB_stat));
	rc = mdb_stat(txn, dbi, stat);
	if (rc != MDB_SUCCESS) {
		efree(stat);
		php_error_docref(NULL, E_NOTICE, "mdb_stat: %d", rc);
		RETURN_NULL();
	}

	RETURN_RES(zend_register_resource(stat, le_lmdb_stat));
}

PHP_FUNCTION(mdb_dbi_flags)
{
	zval *txn_zv, *dbi_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	unsigned int flags = 0;
	int rc;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	rc = mdb_dbi_flags(txn, dbi, &flags);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_dbi_flags: %d", rc);
		RETURN_LONG(-1);
	}

	RETURN_LONG(flags);
}

PHP_FUNCTION(mdb_set_relctx)
{
	zval *txn_zv, *dbi_zv;
	char *value;
	size_t value_len;
	MDB_txn *txn;
	MDB_dbi dbi;
	char *copy;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_STRING(value, value_len)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	copy = estrndup(value, value_len);
	RETURN_LONG(mdb_set_relctx(txn, dbi, copy));
}

PHP_FUNCTION(mdb_get)
{
	zval *txn_zv, *dbi_zv, *key_zv, *data_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val *key, *data;
	int rc;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_RESOURCE(key_zv)
		Z_PARAM_RESOURCE(data_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	key = php_lmdb_val_from_zval(key_zv);
	data = php_lmdb_val_from_zval(data_zv);
	if (!txn || !key || !data) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	rc = mdb_get(txn, dbi, key, data);
	RETURN_LONG(rc);
}

PHP_FUNCTION(mdb_put)
{
	zval *txn_zv, *dbi_zv, *key_zv, *data_zv;
	zend_long flags;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val *key, *data;

	ZEND_PARSE_PARAMETERS_START(5, 5)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_RESOURCE(key_zv)
		Z_PARAM_RESOURCE(data_zv)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	key = php_lmdb_val_from_zval(key_zv);
	data = php_lmdb_val_from_zval(data_zv);
	if (!txn || !key || !data) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	RETURN_LONG(mdb_put(txn, dbi, key, data, (unsigned int) flags));
}

PHP_FUNCTION(mdb_del)
{
	zval *txn_zv, *dbi_zv, *key_zv, *data_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val *key, *data = NULL;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_RESOURCE(key_zv)
		Z_PARAM_ZVAL(data_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	key = php_lmdb_val_from_zval(key_zv);
	if (!txn || !key) {
		RETURN_THROWS();
	}
	if (Z_TYPE_P(data_zv) != IS_NULL) {
		data = php_lmdb_val_from_zval(data_zv);
		if (!data) {
			RETURN_THROWS();
		}
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	RETURN_LONG(mdb_del(txn, dbi, key, data));
}

PHP_FUNCTION(mdb_cursor_open)
{
	zval *txn_zv, *dbi_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_cursor *cursor = NULL;
	int rc;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	if (!txn) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	rc = mdb_cursor_open(txn, dbi, &cursor);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_cursor_open: %d", rc);
		RETURN_NULL();
	}

	RETURN_RES(zend_register_resource(cursor, le_lmdb_cursor));
}

PHP_FUNCTION(mdb_cursor_close)
{
	zval *cursor_zv;
	MDB_cursor *cursor;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(cursor_zv)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	if (!cursor) {
		RETURN_THROWS();
	}

	mdb_cursor_close(cursor);
	RETURN_NULL();
}

PHP_FUNCTION(mdb_cursor_renew)
{
	zval *txn_zv, *cursor_zv;
	MDB_txn *txn;
	MDB_cursor *cursor;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(cursor_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	if (!txn || !cursor) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_cursor_renew(txn, cursor));
}

PHP_FUNCTION(mdb_cursor_txn)
{
	zval *cursor_zv;
	MDB_cursor *cursor;
	MDB_txn *txn;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(cursor_zv)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	if (!cursor) {
		RETURN_THROWS();
	}

	txn = mdb_cursor_txn(cursor);
	RETURN_RES(zend_register_resource(txn, le_lmdb_txn));
}

PHP_FUNCTION(mdb_cursor_dbi)
{
	zval *cursor_zv;
	MDB_cursor *cursor;
	php_lmdb_dbi *dbi_res;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(cursor_zv)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	if (!cursor) {
		RETURN_THROWS();
	}

	dbi_res = emalloc(sizeof(php_lmdb_dbi));
	dbi_res->dbi = mdb_cursor_dbi(cursor);
	RETURN_RES(zend_register_resource(dbi_res, le_lmdb_dbi));
}

PHP_FUNCTION(mdb_cursor_get)
{
	zval *cursor_zv, *key_zv, *data_zv;
	zend_long op;
	MDB_cursor *cursor;
	MDB_val *key, *data;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(cursor_zv)
		Z_PARAM_RESOURCE(key_zv)
		Z_PARAM_RESOURCE(data_zv)
		Z_PARAM_LONG(op)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	key = php_lmdb_val_from_zval(key_zv);
	data = php_lmdb_val_from_zval(data_zv);
	if (!cursor || !key || !data) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_cursor_get(cursor, key, data, (MDB_cursor_op) op));
}

PHP_FUNCTION(mdb_cursor_put)
{
	zval *cursor_zv, *key_zv, *data_zv;
	zend_long flags;
	MDB_cursor *cursor;
	MDB_val *key, *data;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(cursor_zv)
		Z_PARAM_RESOURCE(key_zv)
		Z_PARAM_RESOURCE(data_zv)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	key = php_lmdb_val_from_zval(key_zv);
	data = php_lmdb_val_from_zval(data_zv);
	if (!cursor || !key || !data) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_cursor_put(cursor, key, data, (unsigned int) flags));
}

PHP_FUNCTION(mdb_cursor_del)
{
	zval *cursor_zv;
	zend_long flags;
	MDB_cursor *cursor;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_RESOURCE(cursor_zv)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	if (!cursor) {
		RETURN_THROWS();
	}

	RETURN_LONG(mdb_cursor_del(cursor, (unsigned int) flags));
}

PHP_FUNCTION(mdb_cursor_count)
{
	zval *cursor_zv;
	MDB_cursor *cursor;
	size_t count = 0;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(cursor_zv)
	ZEND_PARSE_PARAMETERS_END();

	cursor = php_lmdb_cursor_from_zval(cursor_zv);
	if (!cursor) {
		RETURN_THROWS();
	}

	rc = mdb_cursor_count(cursor, &count);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_cursor_count: %d", rc);
		RETURN_LONG(-1);
	}

	RETURN_LONG((zend_long) count);
}

PHP_FUNCTION(mdb_cmp)
{
	zval *txn_zv, *dbi_zv, *a_zv, *b_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val *a, *b;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_RESOURCE(a_zv)
		Z_PARAM_RESOURCE(b_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	a = php_lmdb_val_from_zval(a_zv);
	b = php_lmdb_val_from_zval(b_zv);
	if (!txn || !a || !b) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	RETURN_LONG(mdb_cmp(txn, dbi, a, b));
}

PHP_FUNCTION(mdb_dcmp)
{
	zval *txn_zv, *dbi_zv, *a_zv, *b_zv;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_val *a, *b;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_RESOURCE(txn_zv)
		Z_PARAM_RESOURCE(dbi_zv)
		Z_PARAM_RESOURCE(a_zv)
		Z_PARAM_RESOURCE(b_zv)
	ZEND_PARSE_PARAMETERS_END();

	txn = php_lmdb_txn_from_zval(txn_zv);
	a = php_lmdb_val_from_zval(a_zv);
	b = php_lmdb_val_from_zval(b_zv);
	if (!txn || !a || !b) {
		RETURN_THROWS();
	}

	dbi = php_lmdb_dbi_from_zval(dbi_zv);
	RETURN_LONG(mdb_dcmp(txn, dbi, a, b));
}

PHP_FUNCTION(mdb_reader_check)
{
	zval *env_zv;
	MDB_env *env;
	int dead = 0;
	int rc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(env_zv)
	ZEND_PARSE_PARAMETERS_END();

	env = php_lmdb_env_from_zval(env_zv);
	if (!env) {
		RETURN_THROWS();
	}

	rc = mdb_reader_check(env, &dead);
	if (rc != MDB_SUCCESS) {
		php_error_docref(NULL, E_NOTICE, "mdb_reader_check: %d", rc);
		RETURN_LONG(-1);
	}

	RETURN_LONG(dead);
}

PHP_FUNCTION(mdb_reader_list)
{
	php_error_docref(NULL, E_WARNING, "mdb_reader_list callback binding is not implemented");
	RETURN_LONG(-1);
}

PHP_FUNCTION(mdb_val_create)
{
	zend_string *input;
	php_lmdb_val *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(input)
	ZEND_PARSE_PARAMETERS_END();

	value = emalloc(sizeof(php_lmdb_val));
	value->storage = zend_string_copy(input);
	value->val.mv_size = ZSTR_LEN(value->storage) + 1;
	value->val.mv_data = ZSTR_VAL(value->storage);

	RETURN_RES(zend_register_resource(value, le_lmdb_val));
}

PHP_FUNCTION(mdb_val_size)
{
	zval *value_zv;
	MDB_val *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(value_zv)
	ZEND_PARSE_PARAMETERS_END();

	value = php_lmdb_val_from_zval(value_zv);
	if (!value) {
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) value->mv_size);
}

PHP_FUNCTION(mdb_val_data)
{
	zval *value_zv;
	MDB_val *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(value_zv)
	ZEND_PARSE_PARAMETERS_END();

	value = php_lmdb_val_from_zval(value_zv);
	if (!value) {
		RETURN_THROWS();
	}
	if (!value->mv_data) {
		RETURN_NULL();
	}

	RETURN_STRINGL((char *) value->mv_data, value->mv_size);
}

#define PHP_LMDB_STAT_GETTER(name, field) \
PHP_FUNCTION(name) \
{ \
	zval *stat_zv; \
	MDB_stat *stat; \
	ZEND_PARSE_PARAMETERS_START(1, 1) \
		Z_PARAM_RESOURCE(stat_zv) \
	ZEND_PARSE_PARAMETERS_END(); \
	stat = (MDB_stat *) zend_fetch_resource_ex(stat_zv, "MDB_stat", le_lmdb_stat); \
	if (!stat) { \
		RETURN_THROWS(); \
	} \
	RETURN_LONG((zend_long) stat->field); \
}

PHP_LMDB_STAT_GETTER(mdb_stat_psize, ms_psize)
PHP_LMDB_STAT_GETTER(mdb_stat_depth, ms_depth)
PHP_LMDB_STAT_GETTER(mdb_stat_branch_pages, ms_branch_pages)
PHP_LMDB_STAT_GETTER(mdb_stat_leaf_pages, ms_leaf_pages)
PHP_LMDB_STAT_GETTER(mdb_stat_overflow_pages, ms_overflow_pages)
PHP_LMDB_STAT_GETTER(mdb_stat_entries, ms_entries)

#define PHP_LMDB_INFO_GETTER_LONG(name, field) \
PHP_FUNCTION(name) \
{ \
	zval *info_zv; \
	MDB_envinfo *info; \
	ZEND_PARSE_PARAMETERS_START(1, 1) \
		Z_PARAM_RESOURCE(info_zv) \
	ZEND_PARSE_PARAMETERS_END(); \
	info = (MDB_envinfo *) zend_fetch_resource_ex(info_zv, "MDB_envinfo", le_lmdb_envinfo); \
	if (!info) { \
		RETURN_THROWS(); \
	} \
	RETURN_LONG((zend_long) info->field); \
}

PHP_FUNCTION(mdb_info_mapaddr)
{
	zval *info_zv;
	MDB_envinfo *info;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_RESOURCE(info_zv)
	ZEND_PARSE_PARAMETERS_END();

	info = (MDB_envinfo *) zend_fetch_resource_ex(info_zv, "MDB_envinfo", le_lmdb_envinfo);
	if (!info) {
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) (intptr_t) info->me_mapaddr);
}

PHP_LMDB_INFO_GETTER_LONG(mdb_info_mapsize, me_mapsize)
PHP_LMDB_INFO_GETTER_LONG(mdb_info_last_pgno, me_last_pgno)
PHP_LMDB_INFO_GETTER_LONG(mdb_info_last_txnid, me_last_txnid)
PHP_LMDB_INFO_GETTER_LONG(mdb_info_maxreaders, me_maxreaders)
PHP_LMDB_INFO_GETTER_LONG(mdb_info_numreaders, me_numreaders)

PHP_FUNCTION(new_mdb_val)
{
	php_lmdb_val *value;

	ZEND_PARSE_PARAMETERS_NONE();

	value = ecalloc(1, sizeof(php_lmdb_val));
	RETURN_RES(zend_register_resource(value, le_lmdb_val));
}

PHP_FUNCTION(new_mdb_stat)
{
	MDB_stat *stat;

	ZEND_PARSE_PARAMETERS_NONE();

	stat = ecalloc(1, sizeof(MDB_stat));
	RETURN_RES(zend_register_resource(stat, le_lmdb_stat));
}

PHP_FUNCTION(new_mdb_envinfo)
{
	MDB_envinfo *info;

	ZEND_PARSE_PARAMETERS_NONE();

	info = ecalloc(1, sizeof(MDB_envinfo));
	RETURN_RES(zend_register_resource(info, le_lmdb_envinfo));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_lmdb_void, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry lmdb_php_functions[] = {
	ZEND_FE(mdb_version, arginfo_lmdb_void)
	ZEND_FE(mdb_strerror, arginfo_lmdb_void)
	ZEND_FE(mdb_env_create, arginfo_lmdb_void)
	ZEND_FE(mdb_env_open, arginfo_lmdb_void)
	ZEND_FE(mdb_env_copy, arginfo_lmdb_void)
	ZEND_FE(mdb_env_copyfd, arginfo_lmdb_void)
	ZEND_FE(mdb_env_copy2, arginfo_lmdb_void)
	ZEND_FE(mdb_env_copyfd2, arginfo_lmdb_void)
	ZEND_FE(mdb_env_stat, arginfo_lmdb_void)
	ZEND_FE(mdb_env_info, arginfo_lmdb_void)
	ZEND_FE(mdb_env_sync, arginfo_lmdb_void)
	ZEND_FE(mdb_env_close, arginfo_lmdb_void)
	ZEND_FE(mdb_env_set_flags, arginfo_lmdb_void)
	ZEND_FE(mdb_env_get_flags, arginfo_lmdb_void)
	ZEND_FE(mdb_env_get_path, arginfo_lmdb_void)
	ZEND_FE(mdb_env_get_fd, arginfo_lmdb_void)
	ZEND_FE(mdb_env_set_mapsize, arginfo_lmdb_void)
	ZEND_FE(mdb_env_set_maxreaders, arginfo_lmdb_void)
	ZEND_FE(mdb_env_get_maxreaders, arginfo_lmdb_void)
	ZEND_FE(mdb_env_set_maxdbs, arginfo_lmdb_void)
	ZEND_FE(mdb_env_get_maxkeysize, arginfo_lmdb_void)
	ZEND_FE(mdb_env_set_userctx, arginfo_lmdb_void)
	ZEND_FE(mdb_env_get_userctx, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_begin, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_env, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_id, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_commit, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_abort, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_reset, arginfo_lmdb_void)
	ZEND_FE(mdb_txn_renew, arginfo_lmdb_void)
	ZEND_FE(mdb_dbi_open, arginfo_lmdb_void)
	ZEND_FE(mdb_dbi_close, arginfo_lmdb_void)
	ZEND_FE(mdb_drop, arginfo_lmdb_void)
	ZEND_FE(mdb_stat, arginfo_lmdb_void)
	ZEND_FE(mdb_dbi_flags, arginfo_lmdb_void)
	ZEND_FE(mdb_set_relctx, arginfo_lmdb_void)
	ZEND_FE(mdb_get, arginfo_lmdb_void)
	ZEND_FE(mdb_put, arginfo_lmdb_void)
	ZEND_FE(mdb_del, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_open, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_close, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_renew, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_txn, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_dbi, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_get, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_put, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_del, arginfo_lmdb_void)
	ZEND_FE(mdb_cursor_count, arginfo_lmdb_void)
	ZEND_FE(mdb_cmp, arginfo_lmdb_void)
	ZEND_FE(mdb_dcmp, arginfo_lmdb_void)
	ZEND_FE(mdb_reader_check, arginfo_lmdb_void)
	ZEND_FE(mdb_reader_list, arginfo_lmdb_void)
	ZEND_FE(mdb_val_create, arginfo_lmdb_void)
	ZEND_FE(mdb_val_size, arginfo_lmdb_void)
	ZEND_FE(mdb_val_data, arginfo_lmdb_void)
	ZEND_FE(mdb_stat_psize, arginfo_lmdb_void)
	ZEND_FE(mdb_stat_depth, arginfo_lmdb_void)
	ZEND_FE(mdb_stat_branch_pages, arginfo_lmdb_void)
	ZEND_FE(mdb_stat_leaf_pages, arginfo_lmdb_void)
	ZEND_FE(mdb_stat_overflow_pages, arginfo_lmdb_void)
	ZEND_FE(mdb_stat_entries, arginfo_lmdb_void)
	ZEND_FE(mdb_info_mapaddr, arginfo_lmdb_void)
	ZEND_FE(mdb_info_mapsize, arginfo_lmdb_void)
	ZEND_FE(mdb_info_last_pgno, arginfo_lmdb_void)
	ZEND_FE(mdb_info_last_txnid, arginfo_lmdb_void)
	ZEND_FE(mdb_info_maxreaders, arginfo_lmdb_void)
	ZEND_FE(mdb_info_numreaders, arginfo_lmdb_void)
	ZEND_FE(new_mdb_val, arginfo_lmdb_void)
	ZEND_FE(new_mdb_stat, arginfo_lmdb_void)
	ZEND_FE(new_mdb_envinfo, arginfo_lmdb_void)
	ZEND_FE_END
};

PHP_MINIT_FUNCTION(lmdb_php)
{
	le_lmdb_env = zend_register_list_destructors_ex(NULL, NULL, "MDB_env", module_number);
	le_lmdb_txn = zend_register_list_destructors_ex(NULL, NULL, "MDB_txn", module_number);
	le_lmdb_dbi = zend_register_list_destructors_ex(php_lmdb_dbi_dtor, NULL, "MDB_dbi", module_number);
	le_lmdb_cursor = zend_register_list_destructors_ex(NULL, NULL, "MDB_cursor", module_number);
	le_lmdb_val = zend_register_list_destructors_ex(php_lmdb_val_dtor, NULL, "MDB_val", module_number);
	le_lmdb_stat = zend_register_list_destructors_ex(php_lmdb_stat_dtor, NULL, "MDB_stat", module_number);
	le_lmdb_envinfo = zend_register_list_destructors_ex(php_lmdb_envinfo_dtor, NULL, "MDB_envinfo", module_number);
	REGISTER_LONG_CONSTANT("MDB_VERSION_MAJOR", MDB_VERSION_MAJOR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_VERSION_MINOR", MDB_VERSION_MINOR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_VERSION_PATCH", MDB_VERSION_PATCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_VERSION_FULL", MDB_VERSION_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MDB_VERSION_DATE", MDB_VERSION_DATE, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MDB_VERSION_STRING", MDB_VERSION_STRING, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("MDB_FIXEDMAP", MDB_FIXEDMAP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOSUBDIR", MDB_NOSUBDIR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOSYNC", MDB_NOSYNC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_RDONLY", MDB_RDONLY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOMETASYNC", MDB_NOMETASYNC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_WRITEMAP", MDB_WRITEMAP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_MAPASYNC", MDB_MAPASYNC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOTLS", MDB_NOTLS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOLOCK", MDB_NOLOCK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NORDAHEAD", MDB_NORDAHEAD, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOMEMINIT", MDB_NOMEMINIT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_REVERSEKEY", MDB_REVERSEKEY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_DUPSORT", MDB_DUPSORT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_INTEGERKEY", MDB_INTEGERKEY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_DUPFIXED", MDB_DUPFIXED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_INTEGERDUP", MDB_INTEGERDUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_REVERSEDUP", MDB_REVERSEDUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_CREATE", MDB_CREATE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOOVERWRITE", MDB_NOOVERWRITE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NODUPDATA", MDB_NODUPDATA, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_CURRENT", MDB_CURRENT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_RESERVE", MDB_RESERVE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_APPEND", MDB_APPEND, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_APPENDDUP", MDB_APPENDDUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_MULTIPLE", MDB_MULTIPLE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_CP_COMPACT", MDB_CP_COMPACT, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("MDB_FIRST", MDB_FIRST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_FIRST_DUP", MDB_FIRST_DUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_GET_BOTH", MDB_GET_BOTH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_GET_BOTH_RANGE", MDB_GET_BOTH_RANGE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_GET_CURRENT", MDB_GET_CURRENT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_GET_MULTIPLE", MDB_GET_MULTIPLE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_LAST", MDB_LAST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_LAST_DUP", MDB_LAST_DUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NEXT", MDB_NEXT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NEXT_DUP", MDB_NEXT_DUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NEXT_MULTIPLE", MDB_NEXT_MULTIPLE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NEXT_NODUP", MDB_NEXT_NODUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PREV", MDB_PREV, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PREV_DUP", MDB_PREV_DUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PREV_NODUP", MDB_PREV_NODUP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_SET", MDB_SET, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_SET_KEY", MDB_SET_KEY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_SET_RANGE", MDB_SET_RANGE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PREV_MULTIPLE", MDB_PREV_MULTIPLE, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("MDB_SUCCESS", MDB_SUCCESS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_KEYEXIST", MDB_KEYEXIST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_NOTFOUND", MDB_NOTFOUND, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PAGE_NOTFOUND", MDB_PAGE_NOTFOUND, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_CORRUPTED", MDB_CORRUPTED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PANIC", MDB_PANIC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_VERSION_MISMATCH", MDB_VERSION_MISMATCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_INVALID", MDB_INVALID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_MAP_FULL", MDB_MAP_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_DBS_FULL", MDB_DBS_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_READERS_FULL", MDB_READERS_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_TLS_FULL", MDB_TLS_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_TXN_FULL", MDB_TXN_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_CURSOR_FULL", MDB_CURSOR_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_PAGE_FULL", MDB_PAGE_FULL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_MAP_RESIZED", MDB_MAP_RESIZED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_INCOMPATIBLE", MDB_INCOMPATIBLE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_BAD_RSLOT", MDB_BAD_RSLOT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_BAD_TXN", MDB_BAD_TXN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_BAD_VALSIZE", MDB_BAD_VALSIZE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_BAD_DBI", MDB_BAD_DBI, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MDB_LAST_ERRCODE", MDB_LAST_ERRCODE, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(lmdb_php)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "lmdb-php support", "enabled");
	php_info_print_table_row(2, "lmdb-php version", PHP_LMDB_PHP_VERSION);
	php_info_print_table_row(2, "LMDB version", MDB_VERSION_STRING);
	php_info_print_table_end();
}

zend_module_entry lmdb_php_module_entry = {
	STANDARD_MODULE_HEADER,
	"lmdb_php",
	lmdb_php_functions,
	PHP_MINIT(lmdb_php),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(lmdb_php),
	PHP_LMDB_PHP_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_LMDB_PHP
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(lmdb_php)
#endif
