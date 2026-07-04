<?php

namespace Lmdb;

final class Database
{
    public function __construct(
        private Environment $environment,
        private ?string $name = null,
    ) {
    }

    public function name(): ?string
    {
        return $this->name;
    }

    public function get(string $key): ?string
    {
        return $this->environment->read(fn (Transaction $txn) => $txn->database($this->name)->get($key));
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
        $this->environment->write(fn (Transaction $txn) => $txn->database($this->name)->put($key, $value, $flags));
    }

    public function delete(string $key): bool
    {
        return $this->environment->write(fn (Transaction $txn) => $txn->database($this->name)->delete($key));
    }

    public function all(): Cursor
    {
        return new Cursor($this->environment, $this->name);
    }

    public function prefix(string $prefix): Cursor
    {
        return new Cursor($this->environment, $this->name, $prefix);
    }
}
