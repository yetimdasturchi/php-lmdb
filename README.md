# lmdb-php

LMDB bindings for PHP 8 with a simple high-level API

## Install

Debian/Ubuntu dependencies:

```sh
make deps
```

Build and test:

```sh
make
make test
```

Install and enable the extension:

```sh
make install
```

## Helper API

```php
require_once __DIR__ . '/src/Lmdb/autoload.php';

use Lmdb\Environment;

$db = Environment::open(__DIR__ . '/data');

$db->put('name', 'Eshmat');
echo $db->get('name');

$db->write(function ($txn) {
    $users = $txn->database('users');
    $users->put('1', 'Toshmat');
    $users->put('2', 'Boltaboy');
});

foreach ($db->database('users')->all() as $key => $value) {
    echo "$key: $value\n";
}

$db->close();
```

## Direct API

The extension also exposes low-level `mdb_*` functions:

```php
$env = mdb_env_create();
mdb_env_open($env, __DIR__ . '/data', 0, 0664);

$txn = mdb_txn_begin($env, null, 0);
$dbi = mdb_dbi_open($txn, null, 0);

mdb_put($txn, $dbi, mdb_val_create('key'), mdb_val_create('value'), 0);
mdb_txn_commit($txn);

mdb_dbi_close($env, $dbi);
mdb_env_close($env);
```

## Project Layout

- `ext/lmdb_php/` - PHP extension source
- `src/Lmdb/` - helper API
- `tests/direct.php` - direct `mdb_*` test
- `tests/api.php` - helper API test
