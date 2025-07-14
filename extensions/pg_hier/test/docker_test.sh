#!/bin/bash

# Docker Test Runner for pg_hier Extension
# This script runs tests inside a Docker container

set -e

# Configuration
DOCKER_COMPOSE_FILE="../../../docker-compose.yml"
CONTAINER_NAME="postgres_container"
DB_NAME="testdb"

echo "========================================="
echo "    Docker Test Runner for pg_hier      "
echo "========================================="
echo ""

# Check if docker-compose file exists
if [ ! -f "$DOCKER_COMPOSE_FILE" ]; then
    echo "Error: docker-compose.yml not found at $DOCKER_COMPOSE_FILE"
    exit 1
fi

# Start the container if not running
echo "Starting PostgreSQL container..."
cd ../../..
docker-compose up -d db

# Wait for PostgreSQL to be ready
echo "Waiting for PostgreSQL to be ready..."
for i in {1..30}; do
    if docker exec $CONTAINER_NAME pg_isready -U postgres; then
        echo "PostgreSQL is ready!"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "Timeout waiting for PostgreSQL"
        exit 1
    fi
    sleep 2
done

# Create test database
echo "Creating test database..."
docker exec $CONTAINER_NAME createdb -U postgres $DB_NAME 2>/dev/null || echo "Database $DB_NAME already exists"

# Install the extension
echo "Installing pg_hier extension..."
docker exec $CONTAINER_NAME psql -U postgres -d $DB_NAME -c "CREATE EXTENSION IF NOT EXISTS pg_hier;"

# Copy test files to container
echo "Copying test files to container..."
docker cp extensions/pg_hier/test/ $CONTAINER_NAME:/tmp/pg_hier_test/

# Run startup tests
echo "Running startup tests..."
docker exec $CONTAINER_NAME psql -U postgres -d $DB_NAME -f /tmp/pg_hier_test/startup_tests.sql

# Run unit tests
echo "Running unit tests..."
docker exec $CONTAINER_NAME psql -U postgres -d $DB_NAME -f /tmp/pg_hier_test/unit_tests.sql

# Run full test suite
echo "Running full test suite..."
docker exec $CONTAINER_NAME psql -U postgres -d $DB_NAME -f /tmp/pg_hier_test/test_pg_hier.sql

# Copy results back
echo "Copying test results..."
docker exec $CONTAINER_NAME mkdir -p /tmp/test_results
docker cp $CONTAINER_NAME:/tmp/test_results/ ./test_results_docker/ 2>/dev/null || echo "No test results to copy"

echo ""
echo "========================================="
echo "        Docker Tests Completed          "
echo "========================================="
echo ""
echo "Container: $CONTAINER_NAME"
echo "Database: $DB_NAME"
echo "Extension: pg_hier"
echo ""
echo "To run additional tests manually:"
echo "docker exec -it $CONTAINER_NAME psql -U postgres -d $DB_NAME"
echo ""
echo "To stop the container:"
echo "docker-compose down"
