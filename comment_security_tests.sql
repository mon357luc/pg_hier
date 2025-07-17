-- Test cases for comment security validation in pg_hier

-- These queries should be REJECTED with security errors:

-- Test 1: Line comment at beginning
SELECT pg_hier_parse('-- malicious comment
orders { scientific_name }');

-- Test 2: Line comment in middle  
SELECT pg_hier_parse('orders { scientific_name -- inject here
, common_name }');

-- Test 3: Block comment
SELECT pg_hier_parse('orders { /* malicious */ scientific_name }');

-- Test 4: Multiple statements (should also be rejected)
SELECT pg_hier_parse('orders { scientific_name }; DROP TABLE users;');

-- Test 5: Line comment in WHERE clause
SELECT pg_hier_parse('orders [order_id = 1 -- OR 1=1] { scientific_name }');

-- These queries should work fine (no comments):

-- Test 6: Normal query
SELECT pg_hier_parse('orders [order_id = 1] { scientific_name, common_name }');

-- Test 7: Nested query
SELECT pg_hier_parse('orders { 
    scientific_name,
    families { 
        scientific_name,
        species { scientific_name } 
    }
}');

-- Expected behavior:
-- Tests 1-5: Should throw ERROR with message about comments not being allowed
-- Tests 6-7: Should execute successfully and generate proper SQL
