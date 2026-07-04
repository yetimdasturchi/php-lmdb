<?php

require_once __DIR__ . '/../src/Lmdb/autoload.php';

use Lmdb\Environment;
use Lmdb\NotFoundException;
use Lmdb\Options;

$path = __DIR__ . '/api-testdb';

$cleanup = static function () use ($path): void {
    if (!is_dir($path)) {
        return;
    }

    array_map('unlink', glob($path . '/*') ?: []);
    rmdir($path);
};

$fail = static function () use ($cleanup): int {
    $cleanup();

    return 0;
};

$cleanup();

$options = Options::fromArray([
    'map_size' => 10485760,
    'max_dbs' => 8,
    'mode' => 0664,
    'create_directory' => true,
]);

if ($options->getMapSize() !== 10485760 || $options->getMaxDatabases() !== 8) {
    return $fail();
}

$env = Environment::open($path, $options);

$env->put('user:1', 'Eshmat');

if ($env->get('user:1') !== 'Eshmat') {
    return $fail();
}

if ($env->get('missing') !== null) {
    return $fail();
}

try {
    $env->getOrFail('missing');
    return $fail();
} catch (NotFoundException $exception) {
    if ($exception->getCode() !== MDB_NOTFOUND) {
        return $fail();
    }
}

$readValue = $env->read(fn ($txn) => $txn->get('user:1'));

if ($readValue !== 'Eshmat') {
    return $fail();
}

$env->write(function ($txn): void {
    $users = $txn->database('users');
    $users->put('admin:1', 'Toshmat');
    $users->put('admin:2', 'Mamat');
    $users->put('guest:1', 'Boltaboy');
});

if ($env->database('users')->getOrFail('admin:1') !== 'Toshmat') {
    return $fail();
}

if ($env->database('missing-db')->get('anything') !== null) {
    return $fail();
}

$all = [];

foreach ($env->database('users')->all() as $key => $value) {
    $all[$key] = $value;
}

if ($all !== [
    'admin:1' => 'Toshmat',
    'admin:2' => 'Mamat',
    'guest:1' => 'Boltaboy',
]) {
    return $fail();
}

$prefixed = [];

foreach ($env->database('users')->prefix('admin:') as $key => $value) {
    $prefixed[$key] = $value;
}

if ($prefixed !== [
    'admin:1' => 'Toshmat',
    'admin:2' => 'Mamat',
]) {
    return $fail();
}

if ($env->delete('user:1') !== true || $env->get('user:1') !== null) {
    return $fail();
}

if ($env->delete('user:1') !== false) {
    return $fail();
}

$deletedInTxn = $env->write(fn ($txn) => $txn->database('users')->delete('guest:1'));

if ($deletedInTxn !== true || $env->database('users')->get('guest:1') !== null) {
    return $fail();
}

try {
    Options::fromArray(['unknown' => true]);
    return $fail();
} catch (Lmdb\Exception) {
}

$env->close();
$cleanup();

return 1;
