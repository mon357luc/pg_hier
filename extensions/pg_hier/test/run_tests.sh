#!/bin/bash

# pg_hier Extension Test Runner
# This script runs the test suite for the pg_hier extension

set -e

# Configuration
DB_NAME=${POSTGRES_DB:-"testdb"}
DB_USER=${POSTGRES_USER:-"postgres"}
DB_HOST=${POSTGRES_HOST:-"localhost"}
DB_PORT=${POSTGRES_PORT:-"5432"}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}     pg_hier Extension Test Runner      ${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""

# Function to run SQL and capture output
run_sql() {
    local sql_file="$1"
    local output_file="$2"
    
    echo -e "${YELLOW}Running: $sql_file${NC}"
    
    if psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f "$sql_file" > "$output_file" 2>&1; then
        echo -e "${GREEN}✓ Test completed successfully${NC}"
        return 0
    else
        echo -e "${RED}✗ Test failed${NC}"
        echo -e "${RED}Error output:${NC}"
        cat "$output_file"
        return 1
    fi
}

# Check if extension is installed
echo -e "${YELLOW}Checking if pg_hier extension is installed...${NC}"
if psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1 FROM pg_extension WHERE extname = 'pg_hier';" -t | grep -q 1; then
    echo -e "${GREEN}✓ pg_hier extension is installed${NC}"
else
    echo -e "${YELLOW}Installing pg_hier extension...${NC}"
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "CREATE EXTENSION IF NOT EXISTS pg_hier;" || {
        echo -e "${RED}✗ Failed to install pg_hier extension${NC}"
        exit 1
    }
    echo -e "${GREEN}✓ pg_hier extension installed successfully${NC}"
fi

echo ""

# Create output directory
mkdir -p test_results
timestamp=$(date +"%Y%m%d_%H%M%S")

# Run the main test suite
echo -e "${YELLOW}Running main test suite...${NC}"
if run_sql "test/test_pg_hier.sql" "test_results/test_output_$timestamp.log"; then
    echo -e "${GREEN}✓ Main test suite passed${NC}"
    test_status="PASSED"
else
    echo -e "${RED}✗ Main test suite failed${NC}"
    test_status="FAILED"
fi

echo ""

# Run individual test components for detailed analysis
echo -e "${YELLOW}Running individual test components...${NC}"

echo -e "${YELLOW}Running startup tests...${NC}"
run_sql "test/startup_tests.sql" "test_results/startup_tests_$timestamp.log"

echo -e "${YELLOW}Running unit tests...${NC}"
run_sql "test/unit_tests.sql" "test_results/unit_tests_$timestamp.log"

echo -e "${YELLOW}Running edge case tests...${NC}"
run_sql "test/test_edge_cases.sql" "test_results/edge_cases_$timestamp.log"

echo -e "${YELLOW}Running stress and performance tests...${NC}"
run_sql "test/test_stress_performance.sql" "test_results/stress_performance_$timestamp.log"

# Check if taxon schema exists and run taxon tests
echo -e "${YELLOW}Checking for taxon schema...${NC}"
if psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1 FROM information_schema.schemata WHERE schema_name = 'taxon';" -t | grep -q 1; then
    echo -e "${YELLOW}Running taxon integration tests...${NC}"
    run_sql "test/test_taxon_integration.sql" "test_results/taxon_integration_$timestamp.log"
else
    echo -e "${YELLOW}Taxon schema not found, skipping taxon integration tests${NC}"
    echo -e "${YELLOW}To enable taxon tests, run: psql -f ../../taxon_schema.sql${NC}"
fi

echo ""

# Generate test report
report_file="test_results/test_report_$timestamp.html"
echo -e "${YELLOW}Generating test report: $report_file${NC}"

cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>pg_hier Extension Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .header { background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
        .passed { color: green; font-weight: bold; }
        .failed { color: red; font-weight: bold; }
        .warning { color: orange; font-weight: bold; }
        .code { background-color: #f5f5f5; padding: 10px; border-radius: 3px; font-family: monospace; white-space: pre-wrap; }
        .timestamp { color: #666; font-size: 0.9em; }
    </style>
</head>
<body>
    <div class="header">
        <h1>pg_hier Extension Test Report</h1>
        <p class="timestamp">Generated: $(date)</p>
        <p>Status: <span class="$( [[ "$test_status" == "PASSED" ]] && echo "passed" || echo "failed" )">$test_status</span></p>
    </div>
    
    <h2>Test Configuration</h2>
    <ul>
        <li>Database: $DB_NAME</li>
        <li>Host: $DB_HOST:$DB_PORT</li>
        <li>User: $DB_USER</li>
    </ul>
    
    <h2>Test Output</h2>
    <div class="code">$(cat "test_results/test_output_$timestamp.log")</div>
    
    <h2>Extension Information</h2>
    <div class="code">$(psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT * FROM pg_extension WHERE extname = 'pg_hier';" 2>/dev/null || echo "Extension information not available")</div>
</body>
</html>
EOF

echo -e "${GREEN}✓ Test report generated${NC}"

# Summary
echo ""
echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}              Test Summary               ${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""

if [[ "$test_status" == "PASSED" ]]; then
    echo -e "${GREEN}✓ All tests passed successfully!${NC}"
    echo -e "Test output: test_results/test_output_$timestamp.log"
    echo -e "Test report: $report_file"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo -e "Check the output files for details:"
    echo -e "Test output: test_results/test_output_$timestamp.log"
    echo -e "Test report: $report_file"
    exit 1
fi
