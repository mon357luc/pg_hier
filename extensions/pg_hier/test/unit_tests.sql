-- pg_hier Extension Unit Tests
-- This file contains specific unit tests for individual functions

\echo 'Starting pg_hier unit tests...'

-- Function to run a test and report results
CREATE OR REPLACE FUNCTION run_test(test_name TEXT, test_sql TEXT, expected_pattern TEXT DEFAULT NULL)
RETURNS TEXT AS $$
DECLARE
    result TEXT;
    success BOOLEAN := FALSE;
BEGIN
    BEGIN
        EXECUTE test_sql INTO result;
        
        IF expected_pattern IS NULL THEN
            success := TRUE;
        ELSE
            success := result ~ expected_pattern;
        END IF;
        
        IF success THEN
            RAISE NOTICE '[PASS] %: %', test_name, COALESCE(result, 'No result');
            RETURN 'PASS';
        ELSE
            RAISE NOTICE '[FAIL] %: Expected pattern "%" but got "%"', test_name, expected_pattern, result;
            RETURN 'FAIL';
        END IF;
    EXCEPTION
        WHEN OTHERS THEN
            IF expected_pattern = 'ERROR' THEN
                RAISE NOTICE '[PASS] %: Expected error occurred: %', test_name, SQLERRM;
                RETURN 'PASS';
            ELSE
                RAISE NOTICE '[FAIL] %: Unexpected error: %', test_name, SQLERRM;
                RETURN 'FAIL';
            END IF;
    END;
END;
$$ LANGUAGE plpgsql;

-- Initialize test environment
\echo 'Initializing test environment...'
TRUNCATE TABLE pg_hier_detail RESTART IDENTITY CASCADE;
DELETE FROM pg_hier_header;

-- Create simple test hierarchy
SELECT pg_hier_create_hier(
    ARRAY['parent_table', 'child_table'],
    ARRAY[NULL::TEXT, 'parent_id'],
    ARRAY[NULL::TEXT, 'parent_id']
);

\echo 'Running unit tests...'

-- Test pg_hier_create_hier with valid input
SELECT run_test(
    'pg_hier_create_hier_valid',
    'SELECT pg_hier_create_hier(ARRAY[''table1'', ''table2''], ARRAY[NULL::TEXT, ''id''], ARRAY[NULL::TEXT, ''id''])',
    NULL
);

-- Test pg_hier_create_hier with insufficient tables
SELECT run_test(
    'pg_hier_create_hier_insufficient',
    'SELECT pg_hier_create_hier(ARRAY[''table1''], ARRAY[NULL::TEXT], ARRAY[NULL::TEXT])',
    'ERROR'
);

-- Test pg_hier_parse with simple input
SELECT run_test(
    'pg_hier_parse_simple',
    'SELECT LENGTH(pg_hier_parse(''parent_table { id, name, child_table { value } }''))::TEXT',
    '[0-9]+'
);

-- Test pg_hier_parse with empty input
SELECT run_test(
    'pg_hier_parse_empty',
    'SELECT pg_hier_parse('''')',
    'ERROR'
);

-- Test pg_hier_join with valid tables
SELECT run_test(
    'pg_hier_join_valid',
    'SELECT pg_hier_join(''parent_table'', ''child_table'')',
    'FROM.*JOIN'
);

-- Test pg_hier_join with non-existent hierarchy
SELECT run_test(
    'pg_hier_join_nonexistent',
    'SELECT pg_hier_join(''nonexistent1'', ''nonexistent2'')',
    'ERROR'
);

-- Test pg_hier with simple query
SELECT run_test(
    'pg_hier_simple',
    'SELECT pg_hier(''parent_table { id, child_table { value } }'')',
    'jsonb_agg'
);

-- Test pg_hier with invalid syntax
SELECT run_test(
    'pg_hier_invalid_syntax',
    'SELECT pg_hier(''parent_table { missing_brace'')',
    'ERROR'
);

-- Test pg_hier_format with valid SQL
SELECT run_test(
    'pg_hier_format_valid',
    'SELECT pg_hier_format(''SELECT 1 as test'')',
    'row1'
);

-- Test pg_hier_format with invalid SQL
SELECT run_test(
    'pg_hier_format_invalid',
    'SELECT pg_hier_format(''INVALID SQL QUERY'')',
    'ERROR'
);

-- Test complex nested query
SELECT run_test(
    'pg_hier_nested',
    'SELECT LENGTH(pg_hier(''parent_table { id, name, child_table { id, value } }''))::TEXT',
    '[0-9]+'
);

-- Test WHERE clause functionality
SELECT run_test(
    'pg_hier_where_clause',
    'SELECT pg_hier(''parent_table( where { _and { id { _eq: 1 } } } ) { id }'')',
    NULL
);

-- Test operator functions
SELECT run_test(
    'pg_hier_like_operator',
    'SELECT pg_hier(''parent_table( where { name { _like: "test%" } } ) { id }'')',
    NULL
);

SELECT run_test(
    'pg_hier_gt_operator', 
    'SELECT pg_hier(''parent_table( where { id { _gt: 0 } } ) { id }'')',
    NULL
);

SELECT run_test(
    'pg_hier_in_operator',
    'SELECT pg_hier(''parent_table( where { id { _in: (1,2,3) } } ) { id }'')',
    NULL
);

SELECT run_test(
    'pg_hier_is_null_operator',
    'SELECT pg_hier(''parent_table( where { name { _is_null: true } } ) { id }'')',
    NULL
);

-- Test OR operator
SELECT run_test(
    'pg_hier_or_operator',
    'SELECT pg_hier(''parent_table( where { _or { id { _eq: 1 } name { _like: "test%" } } } ) { id }'')',
    NULL
);

-- Performance and edge case tests
SELECT run_test(
    'pg_hier_large_query',
    'SELECT LENGTH(pg_hier(''parent_table { ' || string_agg('col' || i, ', ') || ', child_table { value } }'')))::TEXT FROM generate_series(1, 20) i',
    '[0-9]+'
);

-- Summary
\echo ''
\echo 'Unit tests completed!'
\echo 'Check the output above for individual test results.'

-- Clean up
DROP FUNCTION IF EXISTS run_test(TEXT, TEXT, TEXT);
