# pg_hier Extension Testing System - Complete Setup

## 🎯 Overview

I've created a comprehensive testing framework for your pg_hier PostgreSQL extension that automatically runs when you spin up the extension. Here's what has been implemented:

## 📁 File Structure Created

```
extensions/pg_hier/test/
├── test_pg_hier.sql         # Main comprehensive test suite
├── unit_tests.sql           # Individual function unit tests  
├── startup_tests.sql        # Quick validation tests
├── run_tests.sh            # Linux/macOS test runner
├── run_tests.bat           # Windows test runner
├── docker_test.sh          # Docker environment test runner
└── README.md               # Complete testing documentation
```

## 🚀 Features Implemented

### 1. **Automatic Startup Tests**
- Extension validation runs automatically when installed
- Quick function availability checks
- Basic functionality verification
- User-friendly installation messages

### 2. **Comprehensive Test Suite** (`test_pg_hier.sql`)
- Real-world hierarchical data scenarios
- Complete function testing (pg_hier, pg_hier_parse, pg_hier_join, pg_hier_format)
- WHERE clause functionality with all operators
- Error handling validation
- Performance testing with larger datasets
- Data integrity verification

### 3. **Unit Tests** (`unit_tests.sql`)
- Individual function testing
- Edge case validation
- Error condition testing
- Pass/fail reporting system

### 4. **Multiple Test Runners**
- **Windows**: `run_tests.bat` - Native Windows batch script
- **Linux/macOS**: `run_tests.sh` - Bash script
- **Docker**: `docker_test.sh` - Container-based testing
- **Make targets**: Integrated with existing Makefile

### 5. **Enhanced Makefile**
Added test targets:
```bash
make test          # Full test suite
make test-install  # Install in test database
make test-setup    # Setup test environment
make test-clean    # Clean test environment
make test-quick    # Quick tests
make test-windows  # Windows-specific tests
```

## 🏃‍♂️ How to Run Tests

### Method 1: Automatic (When Installing Extension)
```sql
CREATE EXTENSION pg_hier;
-- Tests run automatically and show status
```

### Method 2: Using Make (Recommended)
```bash
make test
```

### Method 3: Windows Batch Script
```cmd
cd extensions\pg_hier\test
run_tests.bat
```

### Method 4: Manual SQL Execution
```sql
\i test/startup_tests.sql    -- Quick validation
\i test/unit_tests.sql       -- Unit tests  
\i test/test_pg_hier.sql     -- Full suite
```

### Method 5: Docker Environment
```bash
cd extensions/pg_hier/test
./docker_test.sh
```

## 📊 Test Coverage

The test suite covers:

### ✅ **Core Functions**
- `pg_hier()` - Complete hierarchical queries
- `pg_hier_parse()` - Query parsing
- `pg_hier_join()` - JOIN generation
- `pg_hier_format()` - Result formatting
- `pg_hier_create_hier()` - Hierarchy creation

### ✅ **Syntax Features**
- Nested table structures
- WHERE clauses with operators:
  - `_eq`, `_like`, `_gt`, `_lt`
  - `_in`, `_not_in`
  - `_is_null`, `_is_not_null`
  - `_between`, `_not_between`
  - `_exists`, `_not_exists`
  - `_is_true`, `_is_false`
- AND/OR logical operators
- Complex nested conditions

### ✅ **Error Handling**
- Empty input validation
- Invalid syntax detection
- Missing table scenarios
- Malformed queries

### ✅ **Performance & Data Integrity**
- Large dataset handling
- Memory usage validation
- Data consistency checks

## 🎨 Sample Test Data

The tests create realistic hierarchical data:

```
test_item (smartphones, etc.)
├── test_product (electronics category)
    ├── test_electronic_product (brand, warranty)
        ├── test_smartphone (OS, storage, 5G capability)
```

## 📈 Test Output Examples

### Console Output
```
✓ Extension loaded successfully
✓ Required tables exist  
✓ All main functions available
✓ Basic functionality working
✓ Performance test passed

=================================================
    ✓ All startup tests PASSED                   
    pg_hier extension is ready for use!          
=================================================
```

### HTML Reports
Generated in `test_results/` with:
- Timestamp and configuration
- Detailed test output
- Pass/fail status
- Error details if any

## 🔧 Configuration

### Environment Variables
```bash
export POSTGRES_DB=testdb      # Test database name
export POSTGRES_USER=postgres  # Database user
export POSTGRES_HOST=localhost # Database host
export POSTGRES_PORT=5432      # Database port
```

## 🐛 Troubleshooting

### Common Issues & Solutions

1. **Extension not found**
   ```sql
   CREATE EXTENSION pg_hier;
   ```

2. **psql not in PATH** (Windows)
   ```cmd
   set PATH=%PATH%;C:\Program Files\PostgreSQL\16\bin
   ```

3. **Permission issues** (Linux/macOS)
   ```bash
   chmod +x test/run_tests.sh
   ```

4. **Connection refused**
   - Check PostgreSQL is running
   - Verify connection parameters

## 🚀 Next Steps

1. **Run Your First Test**:
   ```cmd
   cd extensions\pg_hier\test
   run_tests.bat
   ```

2. **Integration with Docker**:
   ```bash
   docker-compose up -d
   ./test/docker_test.sh
   ```

3. **CI/CD Integration**:
   - Use `make test` in your build pipeline
   - HTML reports can be archived as artifacts

## 💡 Benefits

- **Immediate Feedback**: Know instantly if extension is working
- **Regression Testing**: Catch issues when making changes
- **Documentation**: Tests serve as usage examples
- **Confidence**: Deploy with certainty that everything works
- **Multiple Environments**: Test locally, in Docker, or CI/CD

The testing system is now ready! When you spin up your extension, you'll get immediate validation that everything is working correctly, and you can run comprehensive tests anytime to ensure your hierarchical query functionality is performing as expected.
