<?php

namespace Lmdb;

use IteratorAggregate;
use Traversable;

final class Cursor implements IteratorAggregate
{
    public function __construct(
        private Environment $environment,
        private ?string $databaseName = null,
        private ?string $prefix = null,
        private ?Transaction $transaction = null,
    ) {
    }

    public static function fromTransaction(Transaction $transaction, ?string $databaseName = null, ?string $prefix = null): self
    {
        return new self($transaction->environment(), $databaseName, $prefix, $transaction);
    }

    public function getIterator(): Traversable
    {
        if ($this->transaction !== null) {
            yield from $this->iterate($this->transaction);
            return;
        }

        $transaction = Transaction::begin($this->environment, true);

        try {
            yield from $this->iterate($transaction);
        } finally {
            $transaction->abort();
        }
    }

    private function iterate(Transaction $transaction): Traversable
    {
        $dbi = $transaction->openDatabase($this->databaseName);

        if ($dbi === null) {
            return;
        }

        $cursor = mdb_cursor_open($transaction->handle(), $dbi);

        if ($cursor === null) {
            throw new Exception('mdb_cursor_open failed');
        }

        $key = TransactionDatabase::value($this->prefix ?? '');
        $data = TransactionDatabase::value('');
        $operation = $this->prefix === null ? MDB_FIRST : MDB_SET_RANGE;

        try {
            while (true) {
                $code = mdb_cursor_get($cursor, $key, $data, $operation);

                if ($code === MDB_NOTFOUND) {
                    return;
                }

                Environment::check($code, 'mdb_cursor_get');

                $keyString = TransactionDatabase::data($key);

                if ($this->prefix !== null && strncmp($keyString, $this->prefix, strlen($this->prefix)) !== 0) {
                    return;
                }

                yield $keyString => TransactionDatabase::data($data);

                $operation = MDB_NEXT;
            }
        } finally {
            mdb_cursor_close($cursor);
        }
    }

}
