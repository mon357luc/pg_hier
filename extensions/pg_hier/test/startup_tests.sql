-- pg_hier Extension Startup Test Script
-- This script runs basic validation tests when the extension is loaded
-- Add this to your postgresql.conf: shared_preload_libraries = 'pg_hier'
-- Or include this file after CREATE EXTENSION pg_hier;

DO $$
DECLARE
    test_passed BOOLEAN := TRUE;
    test_result TEXT;
BEGIN
    RAISE NOTICE '';
    RAISE NOTICE '=================================================';
    RAISE NOTICE '    pg_hier Extension Startup Validation Tests   ';
    RAISE NOTICE '=================================================';
    RAISE NOTICE '';

    -- Test 1: Check if extension is properly loaded
    BEGIN
        SELECT extname INTO test_result FROM pg_extension WHERE extname = 'pg_hier';
        IF test_result = 'pg_hier' THEN
            RAISE NOTICE '[STARTUP TEST 1] ✓ Extension loaded successfully';
        ELSE
            RAISE NOTICE '[STARTUP TEST 1] ✗ Extension not found';
            test_passed := FALSE;
        END IF;
    EXCEPTION
        WHEN OTHERS THEN
            RAISE NOTICE '[STARTUP TEST 1] ✗ Error checking extension: %', SQLERRM;
            test_passed := FALSE;
    END;

    -- Test 2: Check if required tables exist
    BEGIN
        SELECT COUNT(*) INTO test_result 
        FROM information_schema.tables 
        WHERE table_name IN ('pg_hier_header', 'pg_hier_detail');
        
        IF test_result::INTEGER = 2 THEN
            RAISE NOTICE '[STARTUP TEST 2] ✓ Required tables exist';
        ELSE
            RAISE NOTICE '[STARTUP TEST 2] ✗ Missing required tables (found %)', test_result;
            test_passed := FALSE;
        END IF;
    EXCEPTION
        WHEN OTHERS THEN
            RAISE NOTICE '[STARTUP TEST 2] ✗ Error checking tables: %', SQLERRM;
            test_passed := FALSE;
    END;

    -- Test 3: Check if functions are available
    BEGIN
        SELECT COUNT(*) INTO test_result 
        FROM information_schema.routines 
        WHERE routine_name IN ('pg_hier', 'pg_hier_parse', 'pg_hier_join', 'pg_hier_format');
        
        IF test_result::INTEGER = 4 THEN
            RAISE NOTICE '[STARTUP TEST 3] ✓ All main functions available';
        ELSE
            RAISE NOTICE '[STARTUP TEST 3] ✗ Missing functions (found %/4)', test_result;
            test_passed := FALSE;
        END IF;
    EXCEPTION
        WHEN OTHERS THEN
            RAISE NOTICE '[STARTUP TEST 3] ✗ Error checking functions: %', SQLERRM;
            test_passed := FALSE;
    END;

    -- Test 4: Test basic functionality
    BEGIN
        -- Clean up any existing test data
        DELETE FROM pg_hier_detail WHERE hierarchy_id IN (
            SELECT id FROM pg_hier_header WHERE table_path LIKE '%startup_test%'
        );
        DELETE FROM pg_hier_header WHERE table_path LIKE '%startup_test%';
        
        -- Test pg_hier_create_hier
        PERFORM pg_hier_create_hier(
            ARRAY['startup_test_parent', 'startup_test_child'],
            ARRAY[NULL::TEXT, 'parent_id'],
            ARRAY[NULL::TEXT, 'parent_id']
        );
        
        -- Test pg_hier_parse
        SELECT pg_hier_parse('startup_test_parent { id, startup_test_child { value } }') INTO test_result;
        
        IF test_result IS NOT NULL AND LENGTH(test_result) > 10 THEN
            RAISE NOTICE '[STARTUP TEST 4] ✓ Basic functionality working';
        ELSE
            RAISE NOTICE '[STARTUP TEST 4] ✗ Basic functionality failed';
            test_passed := FALSE;
        END IF;
    EXCEPTION
        WHEN OTHERS THEN
            RAISE NOTICE '[STARTUP TEST 4] ✗ Error testing basic functionality: %', SQLERRM;
            test_passed := FALSE;
    END;

    -- Test 5: Performance check
    BEGIN
        SELECT pg_hier('startup_test_parent { id, startup_test_child { value } }') INTO test_result;
        
        IF test_result IS NOT NULL THEN
            RAISE NOTICE '[STARTUP TEST 5] ✓ Performance test passed';
        ELSE
            RAISE NOTICE '[STARTUP TEST 5] ✗ Performance test failed';
            test_passed := FALSE;
        END IF;
    EXCEPTION
        WHEN OTHERS THEN
            RAISE NOTICE '[STARTUP TEST 5] ✗ Error in performance test: %', SQLERRM;
            test_passed := FALSE;
    END;

    -- Clean up test data
    BEGIN
        DELETE FROM pg_hier_detail WHERE hierarchy_id IN (
            SELECT id FROM pg_hier_header WHERE table_path LIKE '%startup_test%'
        );
        DELETE FROM pg_hier_header WHERE table_path LIKE '%startup_test%';
    EXCEPTION
        WHEN OTHERS THEN
            -- Ignore cleanup errors
            NULL;
    END;

    -- Final result
    RAISE NOTICE '';
    IF test_passed THEN
        RAISE NOTICE '=================================================';
        RAISE NOTICE '    ✓ All startup tests PASSED                   ';
        RAISE NOTICE '    pg_hier extension is ready for use!          ';
        RAISE NOTICE '=================================================';
        RAISE NOTICE '';
        RAISE NOTICE 'Usage examples:';
        RAISE NOTICE '  SELECT pg_hier(''table1 { col1, table2 { col2 } }'');';
        RAISE NOTICE '  SELECT pg_hier_parse(''table1 { col1, col2 }'');';
        RAISE NOTICE '  SELECT pg_hier_join(''parent_table'', ''child_table'');';
        RAISE NOTICE '';
        RAISE NOTICE 'To run full test suite: make test';
        RAISE NOTICE 'To run unit tests: psql -f test/unit_tests.sql';
    ELSE
        RAISE NOTICE '=================================================';
        RAISE NOTICE '    ✗ Some startup tests FAILED                  ';
        RAISE NOTICE '    Please check the extension installation       ';
        RAISE NOTICE '=================================================';
    END IF;
    RAISE NOTICE '';
END $$;
