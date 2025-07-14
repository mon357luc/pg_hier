# PostgreSQL extension regression tests
# Uses PGXS build system for standardized testing

# Test configuration
EXTENSION = pg_hier
REGRESS = taxon_test

# Use PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Additional targets for setup and maintenance
.PHONY: setup-test-db create-expected clean-test cleanup-schemas

# Create test database if it doesn't exist
setup-test-db:
	@echo "Setting up test database..."
	createdb contrib_regression || echo "Database already exists"
	@echo "Running all setup scripts..."
	@for setup_script in sql/setup*.sql; do \
		if [ -f "$$setup_script" ]; then \
			echo "Running $$setup_script..."; \
			psql -d contrib_regression -f "$$setup_script" || echo "Script $$setup_script completed with warnings"; \
		fi; \
	done
	@echo "Setup complete"

# Generate expected output files from current test runs
create-expected:
	@echo "Creating expected output files..."
	@for test in $(REGRESS); do \
		if [ -f results/$$test.out ]; then \
			cp results/$$test.out expected/$$test.out; \
			echo "Created expected/$$test.out"; \
		fi; \
	done

# Clean test artifacts  
clean-test:
	rm -rf results/
	rm -f regression.diffs regression.out

# Clean everything including expected files
clean-all: clean-test
	rm -f expected/*.out

# Clean up test schemas and data from database
cleanup-schemas:
	@echo "Cleaning up test schemas..."
	@for cleanup_script in sql/cleanup*.sql; do \
		if [ -f "$$cleanup_script" ]; then \
			echo "Running $$cleanup_script..."; \
			psql -d contrib_regression -f "$$cleanup_script" || echo "Script $$cleanup_script completed with warnings"; \
		fi; \
	done
	@echo "Schema cleanup complete"

# Run specific test
test-%:
	$(pg_regress_installcheck) --dbname=contrib_regression $*

# Run tests with full cleanup
test-clean: installcheck cleanup-schemas

# Help target
help:
	@echo "Available targets:"
	@echo "  installcheck    - Run all regression tests with existing database"
	@echo "  check           - Run all regression tests with temporary database"
	@echo "  test-<name>     - Run specific test (e.g., test-taxon_test)"
	@echo "  setup-test-db   - Create and setup test database (runs all setup*.sql)"
	@echo "  cleanup-schemas - Clean up test schemas (runs all cleanup*.sql)"
	@echo "  test-clean      - Run tests then cleanup schemas"
	@echo "  create-expected - Generate expected output files"
	@echo "  clean-test      - Clean test artifacts"
	@echo "  clean-all       - Clean everything including expected files"
	@echo ""
	@echo "Tests: $(REGRESS)"
