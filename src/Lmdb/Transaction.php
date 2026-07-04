<?php

namespace Lmdb;

final class Transaction
{
    /** @var resource|null */
    private $handle;

    /** @var array<string, mixed> */
    private array $dbis = [];

    private function __construct(
        private Environment $environment,
        $handle,
        private bool $readOnly,
    ) {
        $this->handle = $handle;
    }

    public static function begin(Environment $environment, bool $readOnly): self
    {
        $handle = mdb_txn_begin($environment->handle(), null, $readOnly ? MDB_RDONLY : 0);

        if ($handle === null) {
            throw new Exception('mdb_txn_begin failed');
        }

        return new self($environment, $handle, $readOnly);
    }

    public function database(?string $name = null): TransactionDatabase
    {
        return new TransactionDatabase($this, $name);
    }

    public function get(string $key, ?string $database = null): ?string
    {
        return $this->database($database)->get($key);
    }

    public function getOrFail(string $key, ?string $database = null): string
    {
        return $this->database($database)->getOrFail($key);
    }

    public function put(string $key, string $value, int $flags = 0, ?string $database = null): void
    {
        $this->database($database)->put($key, $value, $flags);
    }

    public function delete(string $key, ?string $database = null): bool
    {
        return $this->database($database)->delete($key);
    }

    public function all(?string $database = null): Cursor
    {
        return Cursor::fromTransaction($this, $database);
    }

    public function prefix(string $prefix, ?string $database = null): Cursor
    {
        return Cursor::fromTransaction($this, $database, $prefix);
    }

    public function commit(): void
    {
        if ($this->handle === null) {
            return;
        }

        if ($this->readOnly) {
            $this->abort();
            return;
        }

        $handle = $this->handle;
        $this->handle = null;
        Environment::check(mdb_txn_commit($handle), 'mdb_txn_commit');
    }

    public function abort(): void
    {
        if ($this->handle !== null) {
            mdb_txn_abort($this->handle);
            $this->handle = null;
        }
    }

    public function __destruct()
    {
        $this->abort();
    }

    public function handle()
    {
        if ($this->handle === null) {
            throw new Exception('LMDB transaction is closed');
        }

        return $this->handle;
    }

    public function environment(): Environment
    {
        return $this->environment;
    }

    public function openDatabase(?string $name)
    {
        $key = $name ?? '';

        if (array_key_exists($key, $this->dbis)) {
            return $this->dbis[$key];
        }

        $flags = $this->readOnly ? 0 : MDB_CREATE;
        $dbi = @mdb_dbi_open($this->handle(), $name, $flags);

        if ($dbi === null) {
            if ($this->readOnly) {
                $this->dbis[$key] = null;
                return null;
            }

            throw new Exception('mdb_dbi_open failed for database: ' . ($name ?? '<default>'));
        }

        return $this->dbis[$key] = $dbi;
    }
}
