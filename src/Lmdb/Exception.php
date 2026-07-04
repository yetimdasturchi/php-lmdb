<?php

namespace Lmdb;

use RuntimeException;

class Exception extends RuntimeException
{
    public static function fromCode(int $code, string $operation): self
    {
        $message = function_exists('mdb_strerror') ? mdb_strerror($code) : 'LMDB error';

        return new self($operation . ' failed: ' . $message, $code);
    }

    public static function extensionMissing(): self
    {
        return new self('The lmdb_php extension is not loaded');
    }
}
