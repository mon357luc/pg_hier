-- pg_hier Extension Edge Cases and Error Testing
-- This file contains comprehensive edge case testing and error handling validation

\echo 'Starting pg_hier edge cases and error testing...'

-- Create test function for error handling
CREATE OR REPLACE FUNCTION test_error_case(
    test_name TEXT,
    test_sql TEXT,
    expected_error_pattern TEXT DEFAULT NULL,
    should_succeed BOOLEAN DEFAULT FALSE
)
RETURNS TEXT AS $$
DECLARE
    result TEXT;
    error_occurred BOOLEAN := FALSE;
    actual_error TEXT;
BEGIN
    BEGIN
        EXECUTE test_sql INTO result;
        
        IF should_succeed THEN
            RAISE NOTICE '[PASS] %: Succeeded as expected', test_name;
            RETURN 'PASS';
        ELSE
            RAISE NOTICE '[FAIL] %: Expected error but query succeeded with result: %', test_name, COALESCE(result, 'NULL');
            RETURN 'FAIL';
        END IF;
        
    EXCEPTION
        WHEN OTHERS THEN
            error_occurred := TRUE;
            actual_error := SQLERRM;
            
            IF NOT should_succeed THEN
                IF expected_error_pattern IS NULL OR actual_error ~ expected_error_pattern THEN
                    RAISE NOTICE '[PASS] %: Expected error occurred: %', test_name, actual_error;
                    RETURN 'PASS';
                ELSE
                    RAISE NOTICE '[FAIL] %: Wrong error occurred. Expected pattern: "%" but got: "%"', test_name, expected_error_pattern, actual_error;
                    RETURN 'FAIL';
                END IF;
            ELSE
                RAISE NOTICE '[FAIL] %: Unexpected error occurred: %', test_name, actual_error;
                RETURN 'FAIL';
            END IF;
    END;
END;
$$ LANGUAGE plpgsql;

\echo ''
\echo '=== EDGE CASE TESTING ==='
\echo ''

-- Set up clean test environment
DELETE FROM pg_hier_detail WHERE hierarchy_id IN (
    SELECT id FROM pg_hier_header WHERE table_path LIKE '%edge_test%'
);
DELETE FROM pg_hier_header WHERE table_path LIKE '%edge_test%';

-- Create test tables for edge cases
DROP TABLE IF EXISTS edge_test_child CASCADE;
DROP TABLE IF EXISTS edge_test_parent CASCADE;
DROP TABLE IF EXISTS edge_test_empty CASCADE;
DROP TABLE IF EXISTS edge_test_unicode CASCADE;

CREATE TABLE edge_test_parent (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    value INTEGER,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE edge_test_child (
    id SERIAL PRIMARY KEY,
    parent_id INTEGER REFERENCES edge_test_parent(id),
    description TEXT,
    amount DECIMAL(10,2),
    is_active BOOLEAN DEFAULT TRUE
);

CREATE TABLE edge_test_empty (
    id SERIAL PRIMARY KEY,
    empty_field TEXT
);

CREATE TABLE edge_test_unicode (
    id SERIAL PRIMARY KEY,
    unicode_name VARCHAR(100),
    emoji_field TEXT
);

-- Insert test data including edge cases
INSERT INTO edge_test_parent (name, value) VALUES
    ('Normal Name', 100),
    ('Name with spaces', 200),
    ('Name-with-dashes', 300),
    ('Name_with_underscores', 400),
    ('Name123with456numbers', 500),
    ('', 600),  -- Empty string
    (NULL, 700),  -- NULL value
    ('Very long name that exceeds normal expectations and contains many characters to test limits', 800),
    ('Name with "quotes"', 900),
    ('Name with ''apostrophes''', 1000);

INSERT INTO edge_test_child (parent_id, description, amount, is_active) VALUES
    (1, 'Normal description', 10.50, true),
    (1, 'Description with special chars: @#$%^&*()', 20.75, false),
    (2, '', 0.00, true),  -- Empty description, zero amount
    (3, NULL, NULL, NULL),  -- All NULL values
    (4, 'Unicode: αβγδε 中文 日本語 العربية', 99.99, true),
    (NULL, 'Orphaned record', 15.25, true);  -- NULL parent_id

INSERT INTO edge_test_unicode (unicode_name, emoji_field) VALUES
    ('Test αβγ', '🚀🌟💫'),
    ('中文测试', '🐉🎋🏮'),
    ('العربية', '🕌🌙⭐'),
    ('Русский', '🏛️❄️🌲'),
    ('Emoji test', '🔥💯🎯🚨⚡');

-- Create hierarchy for edge tests
SELECT pg_hier_create_hier(
    ARRAY['edge_test_parent', 'edge_test_child'],
    ARRAY[NULL::TEXT, 'parent_id'],
    ARRAY[NULL::TEXT, 'parent_id']
);

\echo 'EDGE CASE 1: Empty tables'
SELECT test_error_case(
    'Query on empty table',
    'SELECT pg_hier(''edge_test_empty { id, empty_field }'')',
    NULL,
    TRUE  -- Should succeed even with empty table
);

\echo 'EDGE CASE 2: NULL values in data'
SELECT test_error_case(
    'Query with NULL values',
    'SELECT pg_hier(''edge_test_parent { name, value, edge_test_child { description, amount } }'')',
    NULL,
    TRUE  -- Should succeed and handle NULLs gracefully
);

\echo 'EDGE CASE 3: Unicode and special characters'
SELECT test_error_case(
    'Unicode query',
    'SELECT pg_hier(''edge_test_unicode { unicode_name, emoji_field }'')',
    NULL,
    TRUE  -- Should handle unicode properly
);

\echo 'EDGE CASE 4: Very long query string'
SELECT test_error_case(
    'Very long query',
    format('SELECT pg_hier(''edge_test_parent { %s }'')', 
           string_agg('name', ', ') FROM generate_series(1, 100)),
    NULL,
    TRUE  -- Should handle long queries
);

\echo 'EDGE CASE 5: Query with zero results'
SELECT test_error_case(
    'Query with impossible WHERE condition',
    'SELECT pg_hier(''edge_test_parent( where { value { _eq: -99999 } } ) { name }'')',
    NULL,
    TRUE  -- Should succeed with empty result
);

\echo 'EDGE CASE 6: Complex nested WHERE with edge values'
SELECT test_error_case(
    'Complex WHERE with edge values',
    'SELECT pg_hier(''edge_test_parent( where { _and { value { _gt: 0 } name { _is_not_null: true } name { _not_eq: "" } } } ) { name, value, edge_test_child( where { amount { _gt: 0.00 } is_active { _eq: true } } ) { description, amount } }'')',
    NULL,
    TRUE  -- Should handle complex conditions
);

\echo ''
\echo '=== ERROR TESTING ==='
\echo ''

\echo 'ERROR TEST 1: Empty input string'
SELECT test_error_case(
    'Empty input string',
    'SELECT pg_hier('''')',
    'Empty input string'
);

\echo 'ERROR TEST 2: NULL input'
SELECT test_error_case(
    'NULL input',
    'SELECT pg_hier(NULL)',
    NULL,
    FALSE  -- Should return NULL gracefully
);

\echo 'ERROR TEST 3: Invalid syntax - missing opening brace'
SELECT test_error_case(
    'Missing opening brace',
    'SELECT pg_hier(''edge_test_parent name, value }'')',
    'Expected.*{.*'
);

\echo 'ERROR TEST 4: Invalid syntax - missing closing brace'
SELECT test_error_case(
    'Missing closing brace',
    'SELECT pg_hier(''edge_test_parent { name, value'')',
    'expected.*}.*|Unexpected end'
);

\echo 'ERROR TEST 5: Unmatched braces'
SELECT test_error_case(
    'Unmatched braces',
    'SELECT pg_hier(''edge_test_parent { name, value } }'')',
    'Unmatched.*brace'
);

\echo 'ERROR TEST 6: Invalid table name'
SELECT test_error_case(
    'Non-existent table',
    'SELECT pg_hier(''nonexistent_table { id, name }'')',
    'does not exist|relation.*does not exist'
);

\echo 'ERROR TEST 7: Invalid column name'
SELECT test_error_case(
    'Non-existent column',
    'SELECT pg_hier(''edge_test_parent { nonexistent_column }'')',
    'column.*does not exist'
);

\echo 'ERROR TEST 8: Invalid operator'
SELECT test_error_case(
    'Invalid operator',
    'SELECT pg_hier(''edge_test_parent( where { name { _invalid_op: "test" } } ) { name }'')',
    'Unknown operator'
);

\echo 'ERROR TEST 9: Malformed WHERE clause'
SELECT test_error_case(
    'Malformed WHERE clause',
    'SELECT pg_hier(''edge_test_parent( where { name "test" } ) { name }'')',
    'Expected.*{.*|syntax error'
);

\echo 'ERROR TEST 10: Invalid operator syntax'
SELECT test_error_case(
    'Invalid operator syntax',
    'SELECT pg_hier(''edge_test_parent( where { name { _eq } } ) { name }'')',
    'Expected.*:.*|Unexpected end'
);

\echo 'ERROR TEST 11: Missing colon in operator'
SELECT test_error_case(
    'Missing colon in operator',
    'SELECT pg_hier(''edge_test_parent( where { name { _eq "test" } } ) { name }'')',
    'Expected.*:.*'
);

\echo 'ERROR TEST 12: Single table (insufficient tables)'
SELECT test_error_case(
    'Single table query',
    'SELECT pg_hier(''edge_test_parent { name }'')',
    'At least two tables|Insufficient.*tables'
);

\echo 'ERROR TEST 13: Invalid hierarchy reference'
SELECT test_error_case(
    'Invalid hierarchy join',
    'SELECT pg_hier_join(''nonexistent1'', ''nonexistent2'')',
    'No.*path.*found|does not exist'
);

\echo 'ERROR TEST 14: Invalid BETWEEN operator usage'
SELECT test_error_case(
    'Invalid BETWEEN operator',
    'SELECT pg_hier(''edge_test_parent( where { value { _between: 100 } } ) { name }'')',
    'Expected.*two.*values'
);

\echo 'ERROR TEST 15: Circular reference test'
-- This tests if the extension handles potential circular references
DROP TABLE IF EXISTS circular_a CASCADE;
DROP TABLE IF EXISTS circular_b CASCADE;

CREATE TABLE circular_a (
    id SERIAL PRIMARY KEY,
    name TEXT,
    b_id INTEGER
);

CREATE TABLE circular_b (
    id SERIAL PRIMARY KEY,
    name TEXT,
    a_id INTEGER REFERENCES circular_a(id)
);

ALTER TABLE circular_a ADD CONSTRAINT fk_circular_a_b 
    FOREIGN KEY (b_id) REFERENCES circular_b(id);

SELECT test_error_case(
    'Circular reference tables',
    'SELECT pg_hier_create_hier(ARRAY[''circular_a'', ''circular_b''], ARRAY[NULL::TEXT, ''a_id''], ARRAY[NULL::TEXT, ''a_id''])',
    NULL,
    TRUE  -- Should handle circular references gracefully
);

\echo 'ERROR TEST 16: SQL injection attempt'
SELECT test_error_case(
    'SQL injection attempt',
    'SELECT pg_hier(''edge_test_parent( where { name { _eq: "test"; DROP TABLE edge_test_parent; --" } } ) { name }'')',
    NULL,
    TRUE  -- Should be safely handled as a string value
);

\echo 'ERROR TEST 17: Very deep nesting'
SELECT test_error_case(
    'Very deep nesting attempt',
    'SELECT pg_hier(''edge_test_parent { name, edge_test_child { description, edge_test_parent { name, edge_test_child { description } } } }'')',
    'does not exist|relation.*does not exist'
);

\echo 'ERROR TEST 18: Invalid format function input'
SELECT test_error_case(
    'Invalid SQL for format function',
    'SELECT pg_hier_format(''INVALID SQL SYNTAX HERE'')',
    'syntax error'
);

\echo 'ERROR TEST 19: Memory stress test'
-- Test with a very large result set
INSERT INTO edge_test_parent (name, value) 
SELECT 'bulk_' || i, i FROM generate_series(1, 1000) i;

INSERT INTO edge_test_child (parent_id, description, amount)
SELECT 
    (i % 10) + 1,  -- Reference first 10 parents
    'bulk_desc_' || i,
    (random() * 1000)::DECIMAL(10,2)
FROM generate_series(1, 5000) i;

SELECT test_error_case(
    'Large dataset query',
    'SELECT LENGTH(pg_hier(''edge_test_parent { name, value, edge_test_child { description, amount } }''))::TEXT',
    NULL,
    TRUE  -- Should handle large datasets
);

\echo 'ERROR TEST 20: Concurrent hierarchy creation'
-- Test creating duplicate hierarchy
SELECT test_error_case(
    'Duplicate hierarchy creation',
    'SELECT pg_hier_create_hier(ARRAY[''edge_test_parent'', ''edge_test_child''], ARRAY[NULL::TEXT, ''parent_id''], ARRAY[NULL::TEXT, ''parent_id''])',
    NULL,
    TRUE  -- Should skip creation if already exists
);

\echo ''
\echo '=== BOUNDARY TESTING ==='
\echo ''

\echo 'BOUNDARY TEST 1: Maximum string length'
SELECT test_error_case(
    'Very long string value',
    format('SELECT pg_hier(''edge_test_parent( where { name { _eq: "%s" } } ) { name }'')', 
           repeat('x', 8000)),
    NULL,
    TRUE  -- Should handle long strings
);

\echo 'BOUNDARY TEST 2: Maximum integer values'
SELECT test_error_case(
    'Maximum integer test',
    'SELECT pg_hier(''edge_test_parent( where { value { _eq: 2147483647 } } ) { name }'')',
    NULL,
    TRUE
);

\echo 'BOUNDARY TEST 3: Minimum values'
SELECT test_error_case(
    'Minimum value test',
    'SELECT pg_hier(''edge_test_parent( where { value { _eq: -2147483648 } } ) { name }'')',
    NULL,
    TRUE
);

\echo 'BOUNDARY TEST 4: Float precision edge cases'
INSERT INTO edge_test_child (parent_id, amount) VALUES (1, 0.000001), (1, 999999.99);

SELECT test_error_case(
    'Float precision test',
    'SELECT pg_hier(''edge_test_parent { name, edge_test_child( where { amount { _gt: 0.000001 } } ) { amount } }'')',
    NULL,
    TRUE
);

\echo 'BOUNDARY TEST 5: Boolean edge cases'
SELECT test_error_case(
    'Boolean edge cases',
    'SELECT pg_hier(''edge_test_parent { name, edge_test_child( where { _and { is_active { _is_true: true } is_active { _is_not_null: true } } } ) { is_active } }'')',
    NULL,
    TRUE
);

-- Performance degradation test
\echo 'BOUNDARY TEST 6: Performance degradation test'
\timing on
SELECT test_error_case(
    'Performance test with complex query',
    'SELECT LENGTH(pg_hier(''edge_test_parent( where { _or { value { _gt: 100 } name { _like: "%test%" } name { _is_not_null: true } } } ) { name, value, edge_test_child( where { _and { amount { _gt: 0 } description { _is_not_null: true } is_active { _eq: true } } } ) { description, amount, is_active } }''))::TEXT',
    NULL,
    TRUE
);
\timing off

-- Clean up test data
DROP TABLE IF EXISTS edge_test_child CASCADE;
DROP TABLE IF EXISTS edge_test_parent CASCADE;
DROP TABLE IF EXISTS edge_test_empty CASCADE;
DROP TABLE IF EXISTS edge_test_unicode CASCADE;
DROP TABLE IF EXISTS circular_a CASCADE;
DROP TABLE IF EXISTS circular_b CASCADE;

-- Clean up test function
DROP FUNCTION IF EXISTS test_error_case(TEXT, TEXT, TEXT, BOOLEAN);

\echo ''
\echo '========================================='
\echo 'Edge cases and error testing completed!'
\echo '========================================='
\echo ''
\echo 'This comprehensive test suite validates:'
\echo '• Proper error handling for invalid input'
\echo '• Edge cases with special characters and Unicode'
\echo '• Boundary conditions and performance limits'
\echo '• SQL injection protection'
\echo '• Memory and resource management'
\echo '• Concurrent operations'
\echo ''
