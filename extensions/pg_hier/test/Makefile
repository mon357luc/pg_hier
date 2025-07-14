# Test-only Makefile for pg_hier extension

# Regression tests
REGRESS = basic_test functions_test

REGRESS_OPTS = --dbname=contrib_regression_pg_hier

NO_INSTALL = 1

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
