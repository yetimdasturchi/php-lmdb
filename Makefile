PHP ?= php
PHP_VERSION := $(shell $(PHP) -r 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION;')
PHP_CONFIG ?= $(shell if command -v php-config$(PHP_VERSION) >/dev/null 2>&1; then command -v php-config$(PHP_VERSION); else command -v php-config; fi)
PHPIZE ?= $(shell if command -v phpize$(PHP_VERSION) >/dev/null 2>&1; then command -v phpize$(PHP_VERSION); else command -v phpize; fi)

PHP_INCLUDES := $(shell $(PHP_CONFIG) --includes)
PHP_EXTENSION_DIR := $(shell $(PHP_CONFIG) --extension-dir)
PHP_API := $(shell $(PHP) -i | sed -n 's/^PHP API => //p')
PHP_CONFIG_API := $(shell $(PHP_CONFIG) --phpapi)
LMDB_LIB ?= -llmdb
EXT_DIR := ext/lmdb_php
EXT_SOURCES := $(EXT_DIR)/lmdb-php_wrap.c
EXT_HEADERS := $(EXT_DIR)/php_lmdb_php.h
EXT_OUTPUT := lmdb-php.so

all: $(EXT_OUTPUT)

$(EXT_OUTPUT): FORCE $(EXT_SOURCES) $(EXT_HEADERS)
	@test "$(PHP_API)" = "$(PHP_CONFIG_API)" || (echo "PHP and PHP_CONFIG API mismatch: PHP=$(PHP_API), PHP_CONFIG=$(PHP_CONFIG_API)" >&2; exit 1)
	$(CC) -shared -fPIC -DCOMPILE_DL_LMDB_PHP $(PHP_INCLUDES) -I$(EXT_DIR) $(EXT_SOURCES) $(LMDB_LIB) -o $(EXT_OUTPUT)

deps:
	sudo apt-get update
	sudo apt-get install -y build-essential composer php-dev liblmdb-dev

install: $(EXT_OUTPUT)
	sudo install -m 0755 $(EXT_OUTPUT) $(PHP_EXTENSION_DIR)/lmdb-php.so
	sudo install -d /etc/php/$(PHP_VERSION)/mods-available
	echo "extension=lmdb-php.so" | sudo tee /etc/php/$(PHP_VERSION)/mods-available/lmdb-php.ini >/dev/null
	sudo phpenmod -v $(PHP_VERSION) lmdb-php

print-ini:
	@echo "extension=lmdb-php.so"

phpize-build:
	cd $(EXT_DIR) && $(PHPIZE)
	cd $(EXT_DIR) && ./configure --with-lmdb-php
	cd $(EXT_DIR) && $(MAKE)
	cp $(EXT_DIR)/modules/lmdb_php.so $(EXT_OUTPUT)

test: $(EXT_OUTPUT)
	cd tests && ( for p in *php; do printf "%s: " "$$p"; $(PHP) -n -d extension=../lmdb-php.so $$p > /dev/null && echo OK || echo NOT OK; done )

clean:
	rm -f $(EXT_OUTPUT)
	rm -rf $(EXT_DIR)/.libs $(EXT_DIR)/autom4te.cache $(EXT_DIR)/build $(EXT_DIR)/include $(EXT_DIR)/modules
	rm -f $(EXT_DIR)/*.lo $(EXT_DIR)/*.la $(EXT_DIR)/*.o $(EXT_DIR)/Makefile $(EXT_DIR)/Makefile.fragments $(EXT_DIR)/Makefile.objects
	rm -f $(EXT_DIR)/acinclude.m4 $(EXT_DIR)/aclocal.m4 $(EXT_DIR)/config.guess $(EXT_DIR)/config.h $(EXT_DIR)/config.h.in $(EXT_DIR)/config.log $(EXT_DIR)/config.nice $(EXT_DIR)/config.status $(EXT_DIR)/configure $(EXT_DIR)/install-sh $(EXT_DIR)/libtool $(EXT_DIR)/ltmain.sh $(EXT_DIR)/missing $(EXT_DIR)/run-tests.php

.PHONY: FORCE
FORCE:
