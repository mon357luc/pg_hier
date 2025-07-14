-- Test for pg_hier extension functions
-- Test all public functions and their edge cases

-- Create the extension
CREATE EXTENSION IF NOT EXISTS pg_hier;

-- Test that the extension provides the expected objects
-- List all functions provided by pg_hier
SELECT proname, pronargs 
FROM pg_proc p 
JOIN pg_namespace n ON p.pronamespace = n.oid 
WHERE n.nspname = 'public' 
  AND p.oid IN (
    SELECT objid 
    FROM pg_depend 
    WHERE refobjid = (SELECT oid FROM pg_extension WHERE extname = 'pg_hier')
  )
ORDER BY proname;

-- Test any types provided by pg_hier
SELECT typname 
FROM pg_type t 
JOIN pg_namespace n ON t.typnamespace = n.oid 
WHERE n.nspname = 'public' 
  AND t.oid IN (
    SELECT objid 
    FROM pg_depend 
    WHERE refobjid = (SELECT oid FROM pg_extension WHERE extname = 'pg_hier')
  )
ORDER BY typname;

-- Add specific function tests here once you have functions implemented
-- Example: SELECT your_hier_function('test_input');

-- Clean up
DROP EXTENSION pg_hier;
