# pg_hier Extension Test Suite

This directory contains comprehensive test cases for the `pg_hier` PostgreSQL extension, covering functional testing, edge cases, error handling, and real-world usage scenarios.

## Test Structure

### Test Files

1. **taxon_test.sql** - Basic extension functionality tests with taxonomic data
2. **pg_hier_function_test.sql** - Comprehensive function testing for `pg_hier` and `pg_hier_parse`
3. **pg_hier_edge_cases_test.sql** - Edge cases and error handling tests
4. **pg_hier_scenarios_test.sql** - Real-world usage scenarios and performance tests

### Support Files

- **setup_taxon_schema.sql** - Creates test schema and sample taxonomic data
- **cleanup_taxon_schema.sql** - Cleans up test data and schemas
- **Makefile** - PostgreSQL regression test configuration
- **run_tests.sh** - Comprehensive bash test runner (Linux/macOS)
- **run_tests.ps1** - Comprehensive PowerShell test runner (Windows)

## Test Coverage

### Function Tests (`pg_hier_function_test.sql`)
- NULL and empty input handling
- Basic hierarchical queries
- Complex nested queries with multiple conditions
- Multi-level filtering
- Various comparison operators (`_eq`, `_gt`, `_lt`, `_neq`, `_like`, `_ilike`)
- Array operations (`_in`, `_nin`)
- Boolean logic combinations (`_and`, `_or`)
- Aggregations and ordering
- Deep nesting (all taxonomic levels)
- Special character handling
- Pagination and limiting
- Join and format functionality

### Edge Cases (`pg_hier_edge_cases_test.sql`)
- Empty and whitespace-only inputs
- Invalid JSON-like syntax
- Malformed queries (unclosed/mismatched braces)
- Invalid table and field names
- SQL injection attempts
- Unicode character handling
- Memory and performance stress tests
- Boundary condition testing

### Real-World Scenarios (`pg_hier_scenarios_test.sql`)
- Biodiversity research queries
- Conservation biology applications
- Educational tools and field guides
- API backend implementations
- Data migration and export
- Phylogenetic analysis
- Inventory management
- Bioinformatics pipelines
- Mobile app backends
- Scientific publication support
- Citizen science applications
- Ecosystem modeling

## Running Tests

### Prerequisites

1. PostgreSQL server running
2. `pg_config` in PATH
3. Extension compiled and available
4. Test database access permissions

### Quick Start

```bash
# Linux/macOS
cd extensions/pg_hier/test
./run_tests.sh

# Windows PowerShell
cd extensions\pg_hier\test
.\run_tests.ps1
```

### Using Make

```bash
# Run all tests
make installcheck

# Run specific test
make test-pg_hier_function_test

# Setup test database
make setup-test-db

# Clean up test environment
make cleanup-schemas

# Generate expected output files
make create-expected
```

### Test Runner Options

#### Bash Script (`run_tests.sh`)
```bash
./run_tests.sh [OPTIONS]

Options:
  -h, --help              Show help message
  -s, --setup-only        Only setup test environment
  -c, --cleanup-only      Only cleanup test environment
  -t, --test <name>       Run specific test
  -q, --quick             Run basic tests only
  -f, --functions         Run function tests only
  -e, --edge-cases        Run edge case tests only
  -r, --scenarios         Run scenario tests only
  --no-cleanup            Skip cleanup after tests
  --no-setup              Skip setup before tests
```

#### PowerShell Script (`run_tests.ps1`)
```powershell
.\run_tests.ps1 [OPTIONS]

Options:
  -Help                   Show help message
  -SetupOnly              Only setup test environment
  -CleanupOnly            Only cleanup test environment
  -Test <name>            Run specific test
  -Quick                  Run basic tests only
  -Functions              Run function tests only
  -EdgeCases              Run edge case tests only
  -Scenarios              Run scenario tests only
  -NoCleanup              Skip cleanup after tests
  -NoSetup                Skip setup before tests
```

### Example Usage

```bash
# Run only function tests
./run_tests.sh --functions

# Run specific test
./run_tests.sh --test pg_hier_edge_cases_test

# Quick smoke test
./run_tests.sh --quick

# Setup environment only
./run_tests.sh --setup-only

# Run all tests without cleanup
./run_tests.sh --no-cleanup
```

## Test Data

The test suite uses a comprehensive taxonomic hierarchy with:

- **3 Kingdoms**: Animalia, Plantae, Fungi
- **5 Phyla**: Chordata, Arthropoda, Magnoliophyta, Pinophyta, Basidiomycota
- **9 Classes**: Mammalia, Aves, Reptilia, Amphibia, Insecta, Arachnida, Magnoliopsida, Liliopsida, Agaricomycetes
- **10 Orders**: Including Carnivora, Primates, Rodentia, etc.
- **15 Families**: Including Felidae, Canidae, Hominidae, etc.
- **18 Genera**: Including Felis, Panthera, Canis, Homo, etc.
- **100+ Species**: Comprehensive species data with scientific and common names

## Expected Outputs

Expected test outputs are stored in the `expected/` directory. These files contain the expected results for regression testing.

## Log Files

Test execution logs are stored in the `logs/` directory:
- Individual test logs: `<test_name>.log`
- Setup/cleanup logs: `setup_<script>.log`, `cleanup_<script>.log`
- Test report: `test_report.txt`

## Troubleshooting

### Common Issues

1. **Database Connection Errors**
   - Ensure PostgreSQL is running
   - Check database permissions
   - Verify `contrib_regression` database exists

2. **Extension Not Found**
   - Verify extension is compiled: `make && make install`
   - Check PostgreSQL extension directory
   - Ensure proper search path

3. **Test Failures**
   - Check log files in `logs/` directory
   - Compare results with expected outputs
   - Verify test data setup completed successfully

### Debug Mode

For detailed debugging, run tests individually:

```bash
# Run single test with verbose output
pg_regress --dbname=contrib_regression --inputdir=. --outputdir=results pg_hier_function_test
```

### Manual Test Execution

```sql
-- Connect to test database
\c contrib_regression

-- Set search path
SET search_path TO taxon, public;

-- Run individual test queries
SELECT pg_hier('orders { scientific_name, common_name }');
```

## Contributing

When adding new tests:

1. Follow existing naming conventions
2. Include both positive and negative test cases
3. Add appropriate comments and descriptions
4. Update this README if adding new test categories
5. Generate expected output files after verification

## Performance Considerations

- Large hierarchical queries may take significant time
- Edge case tests include stress testing scenarios
- Use `\timing on` in psql for performance measurement
- Monitor memory usage during deep nesting tests

## Test Categories Summary

| Category | Purpose | Test Count | Focus Area |
|----------|---------|------------|------------|
| Basic | Core functionality | ~5 | Extension loading, basic queries |
| Function | Comprehensive API testing | ~20 | All function parameters and combinations |
| Edge Cases | Error handling | ~40 | Invalid inputs, boundary conditions |
| Scenarios | Real-world usage | ~15 | Practical applications, performance |

Total: ~80 test cases covering all aspects of the extension functionality.
