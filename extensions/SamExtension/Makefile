# Define the extension name (must match the .control file)
EXTENSION = sam_ext

# Use MODULE_big for single shared library built from multiple source files
MODULE_big = $(EXTENSION)

DATA = $(wildcard sql/*--*.sql)
MODULES	= $(patsubst %.c,%,$(wildcard src/*.c))
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))

# Path to PostgreSQL's pg_config
PG_CONFIG = pg_config

# Include the PGXS makefile
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
