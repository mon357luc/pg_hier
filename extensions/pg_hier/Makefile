EXTENSION = pg_hier

MODULE_big = $(EXTENSION)

DATA = $(wildcard sql/*--*.sql)
DOCS = $(wildcard doc/*.md)
# MODULES	= $(patsubst %.c,%,$(wildcard src/*.c))
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))

SRCDIR = src
PG_CFLAGS = -I ./include -g
PG_CPPFLAGS = $(PG_CFLAGS)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Test configuration
TEST_DB = testdb
TEST_USER = postgres
TEST_HOST = localhost
TEST_PORT = 5432

# Additional targets for testing
.PHONY: test test-setup test-clean test-install

# Install extension and run tests
test: test-install
	@echo "Running pg_hier extension tests..."
	@chmod +x test/run_tests.sh
	@cd test && POSTGRES_DB=$(TEST_DB) POSTGRES_USER=$(TEST_USER) POSTGRES_HOST=$(TEST_HOST) POSTGRES_PORT=$(TEST_PORT) ./run_tests.sh

# Install extension in test database
test-install: install
	@echo "Installing pg_hier extension in test database..."
	@createdb -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) $(TEST_DB) 2>/dev/null || true
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -c "CREATE EXTENSION IF NOT EXISTS pg_hier;" || echo "Extension already installed"

# Setup test environment
test-setup:
	@echo "Setting up test environment..."
	@createdb -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) $(TEST_DB) 2>/dev/null || echo "Database $(TEST_DB) already exists"

# Clean test environment
test-clean:
	@echo "Cleaning test environment..."
	@dropdb -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) $(TEST_DB) 2>/dev/null || echo "Database $(TEST_DB) does not exist"
	@rm -rf test/test_results

# Run tests on Windows (but inside Docker Linux container)
test-windows:
	@echo "Running pg_hier extension tests..."
	@cd test && bash run_tests.sh

# Run tests inside Docker container (sets proper DB connection)
test-docker:
	@echo "Running pg_hier extension tests in Docker..."
	@cd test && POSTGRES_DB=db POSTGRES_USER=postgres POSTGRES_HOST=localhost POSTGRES_PORT=5432 bash run_tests.sh

# Run tests directly without shell script (Docker-friendly)
test-docker-direct:
	@echo "Running pg_hier extension tests directly in Docker..."
	@echo "Setting up test database..."
	@psql -h localhost -p 5432 -U postgres -d db -c "DROP EXTENSION IF EXISTS pg_hier CASCADE;" || true
	@psql -h localhost -p 5432 -U postgres -d db -c "CREATE EXTENSION IF NOT EXISTS pg_hier;" || true
	@echo "Running startup tests..."
	@psql -h localhost -p 5432 -U postgres -d db -f test/startup_tests.sql
	@echo "Running unit tests..."
	@psql -h localhost -p 5432 -U postgres -d db -f test/unit_tests.sql
	@echo "Running edge case tests..."
	@psql -h localhost -p 5432 -U postgres -d db -f test/test_edge_cases.sql
	@echo "Running performance tests..."
	@psql -h localhost -p 5432 -U postgres -d db -f test/test_stress_performance.sql
	@echo "Setting up taxon schema..."
	@psql -h localhost -p 5432 -U postgres -d db -f ../taxon_schema.sql || echo "Taxon schema may already exist"
	@echo "Running taxon integration tests..."
	@psql -h localhost -p 5432 -U postgres -d db -f test/test_taxon_integration.sql
	@echo "All tests completed successfully!"

# Quick test without full installation
test-quick:
	@echo "Running quick tests..."
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f test/test_pg_hier.sql

# Run specific test suites
test-startup:
	@echo "Running startup tests..."
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f test/startup_tests.sql

test-unit:
	@echo "Running unit tests..."
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f test/unit_tests.sql

test-edge:
	@echo "Running edge case tests..."
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f test/test_edge_cases.sql

test-stress:
	@echo "Running stress and performance tests..."
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f test/test_stress_performance.sql

test-taxon:
	@echo "Setting up taxon schema and running taxon integration tests..."
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f taxon_schema.sql || echo "Taxon schema may already exist"
	@psql -h $(TEST_HOST) -p $(TEST_PORT) -U $(TEST_USER) -d $(TEST_DB) -f test/test_taxon_integration.sql

# Run all tests individually (useful for CI/CD)
test-all-separate: test-startup test-unit test-edge test-stress test-taxon

# Create test documentation
test-docs:
	@echo "Generating test documentation..."
	@echo "# pg_hier Test Results" > test_results/test_summary.md
	@echo "Generated: $(shell date)" >> test_results/test_summary.md
	@echo "" >> test_results/test_summary.md
	@echo "## Test Files" >> test_results/test_summary.md
	@echo "- startup_tests.sql - Quick validation tests" >> test_results/test_summary.md
	@echo "- unit_tests.sql - Individual function tests" >> test_results/test_summary.md
	@echo "- test_edge_cases.sql - Edge cases and error handling" >> test_results/test_summary.md
	@echo "- test_stress_performance.sql - Performance and scalability" >> test_results/test_summary.md
	@echo "- test_taxon_integration.sql - Biological taxonomy integration" >> test_results/test_summary.md
