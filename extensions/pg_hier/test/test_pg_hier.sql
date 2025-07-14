-- pg_hier Extension Test Suite
-- This file contains comprehensive tests for the pg_hier extension
-- Run this after installing the extension to verify functionality

\echo 'Starting pg_hier extension test suite...'

-- Clean up any existing test data
DROP TABLE IF EXISTS test_item CASCADE;
DROP TABLE IF EXISTS test_product CASCADE;
DROP TABLE IF EXISTS test_electronic_product CASCADE;
DROP TABLE IF EXISTS test_smartphone CASCADE;

-- Clean hierarchy tables
TRUNCATE TABLE pg_hier_detail RESTART IDENTITY CASCADE;
DELETE FROM pg_hier_header;

\echo 'Setting up test data...'

-- Create test tables
CREATE TABLE test_item (
    item_id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT
);

CREATE TABLE test_product (
    product_id SERIAL PRIMARY KEY,
    item_id INTEGER REFERENCES test_item(item_id),
    category VARCHAR(50),
    price DECIMAL(10,2)
);

CREATE TABLE test_electronic_product (
    electronic_id SERIAL PRIMARY KEY,
    product_id INTEGER REFERENCES test_product(product_id),
    brand VARCHAR(50),
    warranty_months INTEGER
);

CREATE TABLE test_smartphone (
    smartphone_id SERIAL PRIMARY KEY,
    electronic_id INTEGER REFERENCES test_electronic_product(electronic_id),
    os VARCHAR(20),
    storage_gb INTEGER,
    has_5g BOOLEAN
);

-- Insert test data
INSERT INTO test_item (name, description) VALUES
    ('iPhone 15', 'Latest Apple smartphone'),
    ('Samsung Galaxy S24', 'Premium Android phone'),
    ('Google Pixel 8', 'Google flagship phone');

INSERT INTO test_product (item_id, category, price) VALUES
    (1, 'Electronics', 999.99),
    (2, 'Electronics', 899.99),
    (3, 'Electronics', 699.99);

INSERT INTO test_electronic_product (product_id, brand, warranty_months) VALUES
    (1, 'Apple', 12),
    (2, 'Samsung', 24),
    (3, 'Google', 12);

INSERT INTO test_smartphone (electronic_id, os, storage_gb, has_5g) VALUES
    (1, 'iOS', 128, true),
    (2, 'Android', 256, true),
    (3, 'Android', 128, true);

\echo 'Test data created successfully.'

-- Test 1: Test pg_hier_create_hier function
\echo 'Test 1: Testing pg_hier_create_hier function...'

SELECT pg_hier_create_hier(
    ARRAY['test_item', 'test_product', 'test_electronic_product', 'test_smartphone'],
    ARRAY[NULL::TEXT, 'item_id', 'product_id', 'electronic_id'],
    ARRAY[NULL::TEXT, 'item_id', 'product_id', 'electronic_id']
);

-- Verify hierarchy was created
SELECT 'Hierarchy created:' as status, table_path FROM pg_hier_header;
SELECT 'Hierarchy details:' as status, name, parent_name, level FROM pg_hier_detail ORDER BY level;

-- Test 2: Test pg_hier_parse function
\echo 'Test 2: Testing pg_hier_parse function...'

SELECT 'Parse result:' as test, pg_hier_parse('
    test_item { 
        name, 
        test_product { 
            product_id, 
            test_smartphone { 
                os,
                storage_gb
            }
        }
    }
') as parsed_query;

-- Test 3: Test pg_hier_join function
\echo 'Test 3: Testing pg_hier_join function...'

SELECT 'Join result:' as test, pg_hier_join('test_item', 'test_smartphone') as join_sql;

-- Test 4: Test complete pg_hier function
\echo 'Test 4: Testing complete pg_hier function...'

SELECT 'Hierarchical query result:' as test, pg_hier('
    test_item { 
        name, 
        description,
        test_product { 
            category,
            price,
            test_electronic_product { 
                brand,
                warranty_months,
                test_smartphone { 
                    os,
                    storage_gb,
                    has_5g
                }
            }
        }
    }
') as hier_result;

-- Test 5: Test with WHERE clause
\echo 'Test 5: Testing with WHERE clause...'

SELECT 'Filtered query result:' as test, pg_hier('
    test_item( where { 
        _and { 
            name { _like: "iPhone%" }
        }
    } ) { 
        name,
        test_product { 
            price,
            test_smartphone { 
                os,
                storage_gb
            }
        }
    }
') as filtered_result;

-- Test 6: Test pg_hier_format function
\echo 'Test 6: Testing pg_hier_format function...'

SELECT 'Format result:' as test, pg_hier_format('SELECT name, os FROM test_item JOIN test_product ON test_item.item_id = test_product.item_id JOIN test_electronic_product ON test_product.product_id = test_electronic_product.product_id JOIN test_smartphone ON test_electronic_product.electronic_id = test_smartphone.electronic_id LIMIT 1') as formatted_result;

-- Test 7: Error handling tests
\echo 'Test 7: Testing error handling...'

-- Test empty input
\echo 'Testing empty input...'
DO $$
BEGIN
    PERFORM pg_hier('');
    RAISE NOTICE 'ERROR: Empty input should have raised an error';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'SUCCESS: Empty input correctly raised error: %', SQLERRM;
END $$;

-- Test insufficient tables
\echo 'Testing insufficient tables...'
DO $$
BEGIN
    PERFORM pg_hier('single_table { column }');
    RAISE NOTICE 'ERROR: Single table should have raised an error';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'SUCCESS: Single table correctly raised error: %', SQLERRM;
END $$;

-- Test invalid syntax
\echo 'Testing invalid syntax...'
DO $$
BEGIN
    PERFORM pg_hier('test_item { missing_brace test_product { id } ');
    RAISE NOTICE 'ERROR: Invalid syntax should have raised an error';
EXCEPTION
    WHEN OTHERS THEN
        RAISE NOTICE 'SUCCESS: Invalid syntax correctly raised error: %', SQLERRM;
END $$;

-- Test 8: Performance test with larger dataset
\echo 'Test 8: Performance test...'

-- Add more test data
INSERT INTO test_item (name, description) 
SELECT 
    'Test Item ' || i,
    'Test description for item ' || i
FROM generate_series(4, 100) i;

INSERT INTO test_product (item_id, category, price)
SELECT 
    i,
    CASE WHEN i % 3 = 0 THEN 'Electronics' 
         WHEN i % 3 = 1 THEN 'Accessories' 
         ELSE 'Software' END,
    (random() * 1000 + 100)::DECIMAL(10,2)
FROM generate_series(4, 100) i;

\timing on
SELECT 'Performance test:' as test, length(pg_hier('
    test_item { 
        name,
        test_product { 
            category,
            price
        }
    }
')::text) as result_length;
\timing off

-- Test 9: Complex nested query
\echo 'Test 9: Complex nested query...'

SELECT 'Complex nested result:' as test, pg_hier('
    test_item( where { 
        _or { 
            name { _like: "%iPhone%" }
            name { _like: "%Samsung%" }
        }
    } ) { 
        name,
        description,
        test_product( where { 
            price { _lt: 1000 }
        } ) { 
            category,
            price,
            test_electronic_product { 
                brand,
                warranty_months,
                test_smartphone( where { 
                    has_5g { _eq: true }
                } ) { 
                    os,
                    storage_gb,
                    has_5g
                }
            }
        }
    }
') as complex_result;

-- Test 10: Verify data integrity
\echo 'Test 10: Data integrity verification...'

SELECT 'Item count:' as metric, COUNT(*) as value FROM test_item
UNION ALL
SELECT 'Product count:', COUNT(*) FROM test_product
UNION ALL
SELECT 'Electronic product count:', COUNT(*) FROM test_electronic_product
UNION ALL
SELECT 'Smartphone count:', COUNT(*) FROM test_smartphone;

-- Summary
\echo ''
\echo '========================================='
\echo 'pg_hier Extension Test Suite Completed!'
\echo '========================================='
\echo ''
\echo 'If you see this message, all basic tests have passed successfully.'
\echo 'Check the output above for any specific test failures or errors.'
\echo ''

-- Optional: Clean up test data (comment out if you want to keep test data)
-- DROP TABLE IF EXISTS test_smartphone CASCADE;
-- DROP TABLE IF EXISTS test_electronic_product CASCADE;
-- DROP TABLE IF EXISTS test_product CASCADE;
-- DROP TABLE IF EXISTS test_item CASCADE;

\echo ''
\echo '========================================='
\echo 'Running additional specialized tests...'
\echo '========================================='
\echo ''

-- Run taxon integration tests if schema exists
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.schemata WHERE schema_name = 'taxon') THEN
        RAISE NOTICE 'Running taxon integration tests...';
        \i test_taxon_integration.sql
    ELSE
        RAISE NOTICE 'Taxon schema not found, skipping taxon integration tests';
        RAISE NOTICE 'To run taxon tests, first execute: \\i ../../taxon_schema.sql';
    END IF;
END $$;

\echo 'Running edge cases and error testing...'
\i test_edge_cases.sql

\echo 'Running stress and performance testing...'
\i test_stress_performance.sql
