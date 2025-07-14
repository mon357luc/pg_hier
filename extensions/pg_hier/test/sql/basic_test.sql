-- Basic test for pg_hier extension
-- Test extension creation and basic functionality

-- Create the extension
CREATE EXTENSION IF NOT EXISTS pg_hier;

-- Test basic extension installation
SELECT extname FROM pg_extension WHERE extname = 'pg_hier';

-- Test that extension is properly loaded
\dx pg_hier

-- Test extension version
SELECT extversion FROM pg_extension WHERE extname = 'pg_hier';

-- Clean up
DROP EXTENSION pg_hier;
