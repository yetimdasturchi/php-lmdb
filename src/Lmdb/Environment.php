<?php

namespace Lmdb;

final class Environment
{
    /** @var resource|null */
    private $handle;

    /** @var array<string, Database> */
    private array $databases = [];

    private function __construct($handle)
    {
        $this->handle = $handle;
    }

    public static function open(string $path, Options|array|null $options = null): self
    {
        self::assertExtensionLoaded();

        $options = is_array($options) ? Options::fromArray($options) : ($options ?? Options::new());

        if ($options->shouldCreateDirectory() && !is_dir($path)) {
            if (!mkdir($path, 0775, true) && !is_dir($path)) {
                throw new Exception('Unable to create LMDB directory: ' . $path);
            }
        }

        $env = mdb_env_create();
        if ($env === null) {
            throw new Exception('mdb_env_create failed');
        }

        if ($options->getMapSize() !== null) {
            self::check(mdb_env_set_mapsize($env, $options->getMapSize()), 'mdb_env_set_mapsize');
        }

        if ($options->getMaxReaders() !== null) {
            self::check(mdb_env_set_maxreaders($env, $options->getMaxReaders()), 'mdb_env_set_maxreaders');
        }

        if ($options->getMaxDatabases() > 0) {
            self::check(mdb_env_set_maxdbs($env, $options->getMaxDatabases()), 'mdb_env_set_maxdbs');
        }

        self::check(mdb_env_open($env, $path, $options->getFlags(), $options->getMode()), 'mdb_env_open');

        return new self($env);
    }

    public function database(?string $name = null): Database
    {
        $key = $name ?? '';

        return $this->databases[$key] ??= new Database($this, $name);
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
        return $this->database($database)->all();
    }

    public function prefix(string $prefix, ?string $database = null): Cursor
    {
        return $this->database($database)->prefix($prefix);
    }

    public function read(callable $callback): mixed
    {
        $transaction = Transaction::begin($this, true);

        try {
            $result = $callback($transaction);
            $transaction->abort();

            return $result;
        } catch (\Throwable $exception) {
            $transaction->abort();
            throw $exception;
        }
    }

    public function write(callable $callback): mixed
    {
        $transaction = Transaction::begin($this, false);

        try {
            $result = $callback($transaction);
            $transaction->commit();

            return $result;
        } catch (\Throwable $exception) {
            $transaction->abort();
            throw $exception;
        }
    }

    public function close(): void
    {
        if ($this->handle !== null) {
            mdb_env_close($this->handle);
            $this->handle = null;
        }
    }

    public function __destruct()
    {
        $this->close();
    }

    public function handle()
    {
        if ($this->handle === null) {
            throw new Exception('LMDB environment is closed');
        }

        return $this->handle;
    }

    public static function check(int $code, string $operation): void
    {
        if ($code !== MDB_SUCCESS) {
            throw Exception::fromCode($code, $operation);
        }
    }

    private static function assertExtensionLoaded(): void
    {
        if (!extension_loaded('lmdb_php')) {
            throw Exception::extensionMissing();
        }
    }
}
