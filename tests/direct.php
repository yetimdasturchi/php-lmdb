<?php

$path = __DIR__ . '/direct-testdb';

if (!is_dir($path)) {
    mkdir($path, 0775, true);
}

$env = mdb_env_create();

if ($env === null || mdb_env_open($env, $path, 0, 0664) !== MDB_SUCCESS) {
    return 0;
}

$txn = mdb_txn_begin($env, null, 0);
$dbi = mdb_dbi_open($txn, null, 0);

$key = mdb_val_create('direct:key');
$data = mdb_val_create('direct:value');

if (mdb_put($txn, $dbi, $key, $data, 0) !== MDB_SUCCESS) {
    return 0;
}

if (mdb_txn_commit($txn) !== MDB_SUCCESS) {
    return 0;
}

$txn = mdb_txn_begin($env, null, MDB_RDONLY);
$loaded = mdb_val_create('');

if (mdb_get($txn, $dbi, $key, $loaded) !== MDB_SUCCESS) {
    return 0;
}

if (rtrim(mdb_val_data($loaded), "\0") !== 'direct:value') {
    return 0;
}

$cursor = mdb_cursor_open($txn, $dbi);
$cursorKey = mdb_val_create('');
$cursorData = mdb_val_create('');
$seen = false;

while (mdb_cursor_get($cursor, $cursorKey, $cursorData, MDB_NEXT) === MDB_SUCCESS) {
    if (rtrim(mdb_val_data($cursorKey), "\0") === 'direct:key') {
        $seen = true;
        break;
    }
}

mdb_cursor_close($cursor);
mdb_txn_abort($txn);
mdb_dbi_close($env, $dbi);
mdb_env_close($env);

array_map('unlink', glob($path . '/*') ?: []);
rmdir($path);

return $seen ? 1 : 0;
