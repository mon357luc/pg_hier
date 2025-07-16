-- Edge cases and error handling tests for pg_hier functions


/************************************************************************************************/
-- Create the extension

CREATE EXTENSION IF NOT EXISTS pg_hier;

/************************************************************************************************/
-- Set search path

SET search_path TO taxon, public;

/************************************************************************************************/
-- Set up the hierarchical structure

SELECT pg_hier_create_hier (
    ARRAY ['kingdoms','phyla','classes','orders','families','genera','species'],
    ARRAY [NULL::TEXT,'kingdom_id','phylum_id','class_id','order_id','family_id','genus_id'],
    ARRAY [NULL::TEXT,'kingdom_id','phylum_id','class_id','order_id','family_id','genus_id']
);

/************************************************************************************************/
-- Edge Case 1: Empty query string

SELECT pg_hier('') as result;

/************************************************************************************************/
-- Edge Case 2: Whitespace only

SELECT pg_hier('   ') as result;

/************************************************************************************************/
-- Edge Case 3: Tab and newline whitespace

SELECT pg_hier('	
   
	') as result;

/************************************************************************************************/
-- Edge Case 4: Single character

SELECT pg_hier('a') as result;

/************************************************************************************************/
-- Edge Case 5: Invalid JSON-like syntax

SELECT pg_hier('{ invalid }') as result;

/************************************************************************************************/
-- Edge Case 6: Unclosed braces

SELECT pg_hier('orders { scientific_name') as result;

/************************************************************************************************/
-- Edge Case 7: Mismatched braces

SELECT pg_hier('orders { scientific_name }}') as result;

/************************************************************************************************/
-- Edge Case 8: Invalid table name

SELECT pg_hier('nonexistent_table { field1 }') as result;

/************************************************************************************************/
-- Edge Case 9: Invalid field names

SELECT pg_hier('orders { nonexistent_field }') as result;

/************************************************************************************************/
-- Edge Case 10: Missing field list

SELECT pg_hier('orders') as result;

/************************************************************************************************/
-- Edge Case 11: Empty field list

SELECT pg_hier('orders { }') as result;

/************************************************************************************************/
-- Edge Case 12: Nested empty field list

SELECT pg_hier('orders { families { } }') as result;

/************************************************************************************************/
-- Edge Case 13: Invalid where clause syntax

SELECT pg_hier('orders ( where { invalid syntax } ) { scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 14: Malformed where conditions

SELECT pg_hier('orders ( where { order_id } ) { scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 15: Invalid operator in where clause

SELECT pg_hier('orders ( where { order_id: { _invalid_op: 1 } } ) { scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 16: Missing operator value

SELECT pg_hier('orders ( where { order_id: { _eq: } } ) { scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 17: Invalid nested structure

SELECT pg_hier('orders { families { genera { invalid_nested_table { field } } } }') as result;

/************************************************************************************************/
-- Edge Case 18: Circular reference attempt

SELECT pg_hier('orders { families { orders { scientific_name } } }') as result;

/************************************************************************************************/
-- Edge Case 19: Very long query string

SELECT pg_hier(repeat('orders { scientific_name, common_name, ', 100) || repeat('} ', 100)) as result;

/************************************************************************************************/
-- Edge Case 20: Special characters in field names

SELECT pg_hier('orders { "scientific-name", scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 21: Unicode characters

SELECT pg_hier('orders { scientific_name, "common_näme" }') as result;

/************************************************************************************************/
-- Edge Case 22: SQL injection attempt

SELECT pg_hier('orders { scientific_name; DROP TABLE orders; -- }') as result;


/************************************************************************************************/
-- Edge Case 23: Single table with no relationships

SELECT pg_hier('orders { scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 24: Invalid array syntax in where clause

SELECT pg_hier('orders ( where { order_id: { _in: [1, 2, } } ) { scientific_name }') as result;

/************************************************************************************************/
-- Edge Case 25: Mixed quote types

SELECT pg_hier($$orders ( where { scientific_name: { _eq: "Carnivora" } } ) { scientific_name }$$) as result;

/************************************************************************************************/
-- Test pg_hier_parse edge cases

SELECT pg_hier_parse(NULL) as result;

/************************************************************************************************/
-- Parse Edge Case 2: Empty string

SELECT pg_hier_parse('') as result;

/************************************************************************************************/
-- Parse Edge Case 3: Invalid syntax

SELECT pg_hier_parse('invalid { syntax') as result;

/************************************************************************************************/
-- Test pg_hier_join edge cases

SELECT pg_hier_join(NULL, 'species') as result;

/************************************************************************************************/
-- Join Edge Case 2: NULL child

SELECT pg_hier_join('kingdoms', NULL) as result;

/************************************************************************************************/
-- Join Edge Case 3: Same table

SELECT pg_hier_join('orders', 'orders') as result;

/************************************************************************************************/
-- Join Edge Case 4: Non-existent tables

SELECT pg_hier_join('nonexistent1', 'nonexistent2') as result;

/************************************************************************************************/
-- Join Edge Case 5: Reverse hierarchy

SELECT pg_hier_join('species', 'kingdoms') as result;

/************************************************************************************************/
-- Test pg_hier_format edge cases

SELECT pg_hier_format(NULL) as result;

/************************************************************************************************/
-- Format Edge Case 2: Empty query

SELECT pg_hier_format('') as result;

/************************************************************************************************/
-- Format Edge Case 3: Invalid SQL

SELECT pg_hier_format('INVALID SQL STATEMENT') as result;

/************************************************************************************************/
-- Format Edge Case 4: Query with no results

SELECT pg_hier_format('SELECT * FROM orders WHERE order_id = -999') as result;

/************************************************************************************************/
-- Memory and performance stress tests

SELECT LENGTH(pg_hier_parse($$
kingdoms {
    phyla {
        classes {
            orders {
                families {
                    genera {
                        species {
                            scientific_name
                        }
                    }
                }
            }
        }
    }
}
$$)) as result_length;

/************************************************************************************************/
-- Test with maximum recursion depth

SELECT LENGTH(pg_hier_parse($$
orders {
    order_id, scientific_name, common_name, description,
    families {
        family_id, scientific_name, common_name, description,
        genera {
            genus_id, scientific_name, common_name, description,
            species {
                species_id, scientific_name, common_name, description
            }
        }
    }
}
$$)) as result_length;

/************************************************************************************************/
-- Boundary condition tests

SELECT pg_hier_parse('a { scientific_name }') as result;

/************************************************************************************************/
-- Test very long table name (if valid)

SELECT pg_hier_parse(repeat('a', 63) || ' { scientific_name }') as result;

/************************************************************************************************/
-- Test maximum number of fields

SELECT LENGTH(pg_hier_parse('orders { ' || 
    array_to_string(array_agg('field' || generate_series), ', ') || ' }'
)) as result_length
FROM generate_series(1, 100);

/************************************************************************************************/
-- Clean up

DROP EXTENSION pg_hier CASCADE;
