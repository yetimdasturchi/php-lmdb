<?php

namespace Lmdb;

final class Options
{
    private ?int $mapSize = null;
    private ?int $maxReaders = null;
    private int $maxDatabases = 128;
    private int $mode = 0664;
    private int $flags = 0;
    private bool $createDirectory = true;

    public static function new(): self
    {
        return new self();
    }

    public static function fromArray(array $options): self
    {
        $self = new self();

        foreach ($options as $key => $value) {
            match ($key) {
                'map_size', 'mapSize' => $self->mapSize((int) $value),
                'max_readers', 'maxReaders' => $self->maxReaders((int) $value),
                'max_databases', 'maxDatabases', 'max_dbs', 'maxDbs' => $self->maxDatabases((int) $value),
                'mode' => $self->mode((int) $value),
                'flags' => $self->flags((int) $value),
                'create_directory', 'createDirectory' => $self->createDirectory((bool) $value),
                default => throw new Exception('Unknown LMDB option: ' . $key),
            };
        }

        return $self;
    }

    public function mapSize(int $bytes): self
    {
        $this->mapSize = $bytes;

        return $this;
    }

    public function maxReaders(int $readers): self
    {
        $this->maxReaders = $readers;

        return $this;
    }

    public function maxDatabases(int $databases): self
    {
        $this->maxDatabases = $databases;

        return $this;
    }

    public function mode(int $mode): self
    {
        $this->mode = $mode;

        return $this;
    }

    public function flags(int $flags): self
    {
        $this->flags = $flags;

        return $this;
    }

    public function addFlags(int $flags): self
    {
        $this->flags |= $flags;

        return $this;
    }

    public function readOnly(): self
    {
        return $this->addFlags(MDB_RDONLY);
    }

    public function noSync(): self
    {
        return $this->addFlags(MDB_NOSYNC);
    }

    public function noMetaSync(): self
    {
        return $this->addFlags(MDB_NOMETASYNC);
    }

    public function writeMap(): self
    {
        return $this->addFlags(MDB_WRITEMAP);
    }

    public function createDirectory(bool $create = true): self
    {
        $this->createDirectory = $create;

        return $this;
    }

    public function getMapSize(): ?int
    {
        return $this->mapSize;
    }

    public function getMaxReaders(): ?int
    {
        return $this->maxReaders;
    }

    public function getMaxDatabases(): int
    {
        return $this->maxDatabases;
    }

    public function getMode(): int
    {
        return $this->mode;
    }

    public function getFlags(): int
    {
        return $this->flags;
    }

    public function shouldCreateDirectory(): bool
    {
        return $this->createDirectory;
    }
}
