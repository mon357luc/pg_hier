# PostgreSQL extension regression tests
# Uses PGXS build system for standardized testing

# Test configuration
EXTENSION = pg_hier
REGRESS = $(patsubst sql/%.sql,%,$(wildcard sql/setup*.sql)) \
          $(patsubst sql/%.sql,%,$(filter-out sql/setup% sql/cleanup%,$(wildcard sql/*.sql))) \
          $(patsubst sql/%.sql,%,$(wildcard sql/cleanup*.sql))

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
