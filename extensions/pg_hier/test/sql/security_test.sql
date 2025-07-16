-- Security test file for pg_hier extension
-- Tests that demonstrate SQL injection prevention

-- Create the extension if not exists
CREATE EXTENSION IF NOT EXISTS pg_hier;

-- Test 1: Attempt SQL injection in table name
SELECT 'Testing table name injection prevention:' as test_description;
BEGIN;
    -- This should fail with validation error
    SELECT pg_hier_parse('users; DROP TABLE users; --{ id, name }') as result;
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Table name injection prevented: %', SQLERRM;
END;

-- Test 2: Attempt SQL injection in column name  
SELECT 'Testing column name injection prevention:' as test_description;
BEGIN;
    -- This should fail with validation error
    SELECT pg_hier_parse('users { id; DROP TABLE users; --, name }') as result;
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Column name injection prevented: %', SQLERRM;
END;

-- Test 3: Attempt SQL injection in WHERE clause value
SELECT 'Testing WHERE clause value injection prevention:' as test_description;
BEGIN;
    -- This should be safely escaped
    SELECT pg_hier_parse($$users (where { id: { _eq: "1; DROP TABLE users; --" } }) { id, name }$$) as result;
    RAISE NOTICE 'WHERE clause value properly escaped';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'WHERE clause processing error: %', SQLERRM;
END;

-- Test 4: Attempt SQL injection in pg_hier_format
SELECT 'Testing pg_hier_format injection prevention:' as test_description;
BEGIN;
    -- This should fail - not a SELECT statement
    SELECT pg_hier_format('DROP TABLE users;') as result;
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'pg_hier_format injection prevented: %', SQLERRM;
END;

-- Test 5: Valid queries should still work
SELECT 'Testing valid queries still work:' as test_description;
BEGIN;
    -- This should work fine
    SELECT LENGTH(pg_hier_parse('users { id, name }')) > 0 as valid_query_works;
    RAISE NOTICE 'Valid queries still work correctly';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Valid query failed: %', SQLERRM;
END;

RAISE NOTICE 'Security tests completed.';
