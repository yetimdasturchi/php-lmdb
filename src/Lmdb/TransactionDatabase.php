<?php

namespace Lmdb;

final class TransactionDatabase
{
    public function __construct(
        private Transaction $transaction,
        private ?string $name = null,
    ) {
    }

    public function get(string $key): ?string
    {
        $dbi = $this->transaction->openDatabase($this->name);

        if ($dbi === null) {
            return null;
        }

        $keyValue = self::value($key);
        $dataValue = self::value('');
        $code = mdb_get($this->transaction->handle(), $dbi, $keyValue, $dataValue);

        if ($code === MDB_NOTFOUND) {
            return null;
        }

        Environment::check($code, 'mdb_get');

        return self::data($dataValue);
    }

    public function getOrFail(string $key): string
    {
        $value = $this->get($key);

        if ($value === null) {
            throw new NotFoundException('LMDB key not found: ' . $key, MDB_NOTFOUND);
        }

        return $value;
    }

    public function put(string $key, string $value, int $flags = 0): void
    {
        $dbi = $this->transaction->openDatabase($this->name);

        if ($dbi === null) {
            throw new Exception('Unable to open LMDB database: ' . ($this->name ?? '<default>'));
        }

        Environment::check(
            mdb_put($this->transaction->handle(), $dbi, self::value($key), self::value($value), $flags),
            'mdb_put'
        );
    }

    public function delete(string $key): bool
    {
        $dbi = $this->transaction->openDatabase($this->name);

        if ($dbi === null) {
            return false;
        }

        $code = mdb_del($this->transaction->handle(), $dbi, self::value($key), null);

        if ($code === MDB_NOTFOUND) {
            return false;
        }

        Environment::check($code, 'mdb_del');

        return true;
    }

    public function all(): Cursor
    {
        return Cursor::fromTransaction($this->transaction, $this->name);
    }

    public function prefix(string $prefix): Cursor
    {
        return Cursor::fromTransaction($this->transaction, $this->name, $prefix);
    }

    public static function value(string $value)
    {
        return mdb_val_create($value);
    }

    public static function data($value): string
    {
        return rtrim(mdb_val_data($value), "\0");
    }
}
