EXTENSION = pg_hier

MODULE_big = $(EXTENSION)

DATA = $(wildcard sql/*--*.sql)
DOCS = $(wildcard doc/*.md)
# MODULES	= $(patsubst %.c,%,$(wildcard src/*.c))
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))

SRCDIR = src
PG_CFLAGS = -I ./include -g
PG_CPPFLAGS = $(PG_CFLAGS)

# Custom test target that calls the test subdirectory
test: install
	$(MAKE) -C test installcheck

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
