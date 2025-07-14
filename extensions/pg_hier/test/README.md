# pg_hier Extension Test Suite

This directory contains comprehensive tests for the pg_hier PostgreSQL extension.

## Test Files

### Core Test Files
- **`test_pg_hier.sql`** - Main comprehensive test suite with real-world scenarios
- **`unit_tests.sql`** - Individual function unit tests with specific test cases
- **`startup_tests.sql`** - Quick validation tests that run after extension installation
- **`test_edge_cases.sql`** - Edge cases, error handling, and boundary testing
- **`test_stress_performance.sql`** - Performance, scalability, and stress testing
- **`test_taxon_integration.sql`** - Biological taxonomy schema integration tests

### Test Runners
- **`run_tests.sh`** - Linux/macOS test runner script (Bash)
- **`run_tests.bat`** - Windows test runner script (Batch)

## Running Tests

### Method 1: Using Make (Recommended)
```bash
# Install extension and run full test suite
make test

# Install extension in test database
make test-install

# Set up test environment
make test-setup

# Clean test environment
make test-clean

# Quick test without full installation
make test-quick

# Run tests on Windows
make test-windows

# Run specific test suites
make test-startup     # Quick validation tests
make test-unit        # Individual function tests
make test-edge        # Edge cases and error handling
make test-stress      # Performance and scalability tests
make test-taxon       # Biological taxonomy integration
make test-all-separate # Run all tests individually
```

### Method 2: Manual Test Execution

#### Linux/macOS:
```bash
cd test
chmod +x run_tests.sh
./run_tests.sh
```

#### Windows:
```cmd
cd test
run_tests.bat
```

#### Direct SQL execution:
```sql
-- Quick validation
\i test/startup_tests.sql

-- Unit tests
\i test/unit_tests.sql

-- Full test suite
\i test/test_pg_hier.sql
```

## Test Configuration

### Environment Variables
- `POSTGRES_DB` - Test database name (default: testdb)
- `POSTGRES_USER` - Database user (default: postgres)
- `POSTGRES_HOST` - Database host (default: localhost)
- `POSTGRES_PORT` - Database port (default: 5432)

### Example:
```bash
export POSTGRES_DB=my_test_db
export POSTGRES_USER=myuser
make test
```

## Test Categories

### 1. Installation Tests
- Extension loading validation
- Required tables and functions verification
- Basic functionality checks

### 2. Function Tests
- `pg_hier()` - Complete hierarchical query function
- `pg_hier_parse()` - Query parsing function
- `pg_hier_join()` - Table join generation
- `pg_hier_format()` - Result formatting
- `pg_hier_create_hier()` - Hierarchy creation

### 3. Syntax Tests
- Valid hierarchical query syntax
- WHERE clause functionality
- Operator testing (_eq, _like, _gt, _lt, _in, _is_null, etc.)
- AND/OR logical operators
- Nested query structures

### 4. Error Handling Tests
- Empty input validation
- Invalid syntax detection
- Missing table handling
- Malformed operator usage

### 5. Performance Tests
- Large dataset handling
- Complex nested queries
- Memory usage validation
- Scalability testing
- Concurrent operations

### 6. Taxon Integration Tests
- Biological taxonomy schema integration
- Complex hierarchical relationships (Kingdom → Phylum → Class → Order → Family → Genus → Species)
- Real-world scientific data scenarios
- Cross-kingdom queries

## Test Output

### Console Output
Tests provide real-time feedback with color-coded results:
- ✓ Green: Passed tests
- ✗ Red: Failed tests  
- ⚠ Yellow: Warnings

### HTML Reports
Detailed HTML reports are generated in `test_results/` directory:
- `test_output_YYYYMMDD_HHMMSS.log` - Raw test output
- `test_report_YYYYMMDD_HHMMSS.html` - Formatted HTML report

## Sample Test Data

The test suite creates multiple test datasets:

### 1. Electronics Hierarchy
```
test_item (smartphones, etc.)
├── test_product (electronics category)
    ├── test_electronic_product (brand, warranty)
        ├── test_smartphone (OS, storage, 5G capability)
```

### 2. Biological Taxonomy (Taxon Schema)
```
kingdoms (Animalia, Plantae, Fungi)
├── phyla (Chordata, Arthropoda, etc.)
    ├── classes (Mammalia, Aves, Insecta, etc.)
        ├── orders (Carnivora, Primates, etc.)
            ├── families (Felidae, Hominidae, etc.)
                ├── genera (Felis, Homo, etc.)
                    ├── species (Homo sapiens, Felis catus, etc.)
```

### 3. Performance Test Data
- 4-level hierarchy with 51,000+ records
- Stress testing with large datasets
- Edge case data with special characters, Unicode, NULL values

## Example Test Usage

### Basic Hierarchical Query
```sql
SELECT pg_hier('
    test_item { 
        name, 
        description,
        test_product { 
            category,
            price,
            test_smartphone { 
                os,
                storage_gb,
                has_5g
            }
        }
    }
');
```

### Query with WHERE Clause
```sql
SELECT pg_hier('
    test_item( where { 
        _and { 
            name { _like: "iPhone%" }
        }
    } ) { 
        name,
        test_product { 
            price,
            test_smartphone { 
                os,
                storage_gb
            }
        }
    }
');
```

### Biological Taxonomy Query
```sql
-- Get all mammals and their classification
SELECT pg_hier('
    kingdoms( where { 
        scientific_name { _eq: "Animalia" }
    } ) { 
        scientific_name,
        phyla( where {
            scientific_name { _eq: "Chordata" }
        } ) { 
            classes( where {
                scientific_name { _eq: "Mammalia" }
            } ) { 
                scientific_name,
                orders { 
                    scientific_name,
                    families {
                        scientific_name,
                        genera {
                            scientific_name,
                            species {
                                scientific_name,
                                common_name
                            }
                        }
                    }
                }
            }
        }
    }
');
```

### Edge Case Testing Examples
```sql
-- Test with NULL values and special characters
SELECT pg_hier('
    edge_test_parent( where { 
        _and { 
            value { _gt: 0 } 
            name { _is_not_null: true } 
        }
    } ) { 
        name, 
        value,
        edge_test_child( where { 
            amount { _gt: 0.00 }
        } ) { 
            description, 
            amount 
        }
    }
');
```

## Troubleshooting

### Common Issues

1. **Extension not found**
   ```
   Solution: Ensure extension is installed with CREATE EXTENSION pg_hier;
   ```

2. **psql command not found**
   ```
   Solution: Add PostgreSQL bin directory to PATH
   ```

3. **Permission denied**
   ```
   Solution: chmod +x run_tests.sh (Linux/macOS)
   ```

4. **Connection refused**
   ```
   Solution: Check PostgreSQL is running and connection parameters
   ```

5. **Taxon schema not found**
   ```
   Solution: Run taxon_schema.sql first: psql -f ../../taxon_schema.sql
   ```

6. **Performance tests timing out**
   ```
   Solution: Increase timeout values or test with smaller datasets
   ```

7. **Unicode/special character issues**
   ```
   Solution: Ensure database encoding supports UTF-8
   ```

### Debug Mode
Add `-v ON_ERROR_STOP=1` to psql commands for detailed error information:
```bash
psql -v ON_ERROR_STOP=1 -f test/test_pg_hier.sql
```

## Integration with CI/CD

### GitLab CI Example
```yaml
test:
  script:
    - make test-setup
    - make test
  artifacts:
    reports:
      junit: test_results/*.xml
    paths:
      - test_results/
```

### GitHub Actions Example
```yaml
- name: Run pg_hier tests
  run: |
    make test-setup
    make test
```

## Contributing

When adding new features to pg_hier:

1. Add corresponding tests to `test_pg_hier.sql`
2. Create specific unit tests in `unit_tests.sql`
3. Update this README if new test categories are added
4. Ensure all tests pass before submitting pull requests

## Test Coverage

Current test coverage includes:
- ✅ All core functions
- ✅ Error handling and edge cases
- ✅ Syntax validation
- ✅ WHERE clause operators
- ✅ Nested queries
- ✅ Performance testing
- ✅ Data integrity validation
- ✅ Biological taxonomy integration
- ✅ Unicode and special character handling
- ✅ Boundary condition testing
- ✅ Memory and resource management
- ✅ Concurrent operations
- ✅ SQL injection protection
- ✅ Stress testing with large datasets

For questions or issues with the test suite, please refer to the main project documentation or submit an issue.
