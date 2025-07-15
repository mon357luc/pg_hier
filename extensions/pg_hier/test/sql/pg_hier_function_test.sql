-- Test pg_hier and pg_hier_parse functions with comprehensive test cases

-- Create the extension
CREATE EXTENSION IF NOT EXISTS pg_hier;

-- Set search path to include taxon schema
SET search_path TO taxon, public;

-- Test 1: pg_hier_create_hier - Set up the hierarchical structure
SELECT pg_hier_create_hier (
    ARRAY ['kingdoms','phyla','classes','orders','families','genera','species'],
    ARRAY [NULL::TEXT,'kingdom_id','phylum_id','class_id','order_id','family_id','genus_id'],
    ARRAY [NULL::TEXT,'kingdom_id','phylum_id','class_id','order_id','family_id','genus_id']
);

-- Test 2: NULL input handling
SELECT 'Testing NULL input:' as test_description;
SELECT pg_hier(NULL) as result_null;

-- Test 3: Empty string handling
SELECT 'Testing empty string input:' as test_description;
SELECT pg_hier('') as result_empty;

-- Test 4: Basic hierarchical query - orders with families
SELECT 'Testing basic hierarchical query:' as test_description;
SELECT pg_hier($$ 
orders (
    where {
        _and { 
            _or { 
                _and {
                    order_id: { _eq: 1 }
                }
            }
        }
    }
) { 
    scientific_name,
    common_name,
    families { 
        scientific_name,
        common_name,
        genera (
            where {
                _or {
                    genus_id { _eq: 5 }
                    common_name { _like: '%cat%' }
                }
            }
        ) { 
            scientific_name,
            common_name,
            genus_id, 
            species { 
                scientific_name,
                common_name,
                species_id 
            }
        },
        order_id 
    } 
} 
$$) as hierarchical_result;

-- Test 5: pg_hier_parse - test the parsing function separately
SELECT 'Testing pg_hier_parse function:' as test_description;
SELECT pg_hier_parse($$ 
orders {
    scientific_name,
    common_name,
    families {
        scientific_name,
        common_name
    }
}
$$) as parsed_query;

-- Test 6: Simple single table query
SELECT 'Testing single table query:' as test_description;
SELECT pg_hier_parse($$
kingdoms {
    scientific_name,
    common_name,
    description
}
$$) as single_table_parse;

-- Test 7: Complex nested query with multiple conditions
SELECT 'Testing complex nested query:' as test_description;
SELECT pg_hier_parse($$
kingdoms (
    where {
        _and {
            kingdom_id: { _eq: 1 }
            scientific_name: { _like: 'Animalia' }
        }
    }
) {
    scientific_name,
    common_name,
    phyla (
        where {
            _or {
                phylum_id: { _in: [1, 2] }
                scientific_name: { _ilike: '%chordata%' }
            }
        }
    ) {
        scientific_name,
        common_name,
        classes {
            scientific_name,
            common_name,
            orders {
                scientific_name,
                common_name,
                families {
                    scientific_name,
                    common_name,
                    genera {
                        scientific_name,
                        common_name,
                        species {
                            scientific_name,
                            common_name,
                            species_id
                        }
                    }
                }
            }
        }
    }
}
$$) as complex_nested_parse;

-- Test 8: Query with filtering at multiple levels
SELECT 'Testing multi-level filtering:' as test_description;
SELECT pg_hier($$
families (
    where {
        scientific_name: { _eq: 'Felidae' }
    }
) {
    scientific_name,
    common_name,
    family_id,
    genera (
        where {
            _or {
                scientific_name: { _like: 'Felis' }
                scientific_name: { _like: 'Panthera' }
            }
        }
    ) {
        scientific_name,
        common_name,
        genus_id,
        species (
            where {
                _and {
                    common_name: { _is_not_null: true }
                    scientific_name: { _ilike: '%cat%' }
                }
            }
        ) {
            scientific_name,
            common_name,
            species_id,
            description
        }
    }
}
$$) as felidae_filtered;

-- Test 9: Test with different comparison operators
SELECT 'Testing various comparison operators:' as test_description;
SELECT pg_hier_parse($$
orders (
    where {
        _and {
            order_id: { _gt: 2 }
            order_id: { _lt: 8 }
            scientific_name: { _neq: 'Unknown' }
        }
    }
) {
    order_id,
    scientific_name,
    common_name
}
$$) as operators_test;

-- Test 10: Test with array operations
SELECT 'Testing array operations:' as test_description;
SELECT pg_hier_parse($$
genera (
    where {
        genus_id: { _in: [1, 2, 3, 4, 5] }
    }
) {
    genus_id,
    scientific_name,
    common_name,
    species (
        where {
            species_id: { _nin: [1, 2] }
        }
    ) {
        species_id,
        scientific_name,
        common_name
    }
}
$$) as array_ops_test;

-- Test 11: Test error handling - malformed query
SELECT 'Testing malformed query handling:' as test_description;
SELECT pg_hier_parse($$
invalid_table {
    field1,
    field2
}
$$) as malformed_test;

-- Test 12: Test with aggregations (if supported)
SELECT 'Testing aggregations in parse:' as test_description;
SELECT pg_hier_parse($$
kingdoms {
    scientific_name,
    common_name,
    phyla {
        count: phylum_id,
        scientific_name
    }
}
$$) as aggregation_test;

-- Test 13: Test deep nesting (all levels)
SELECT 'Testing maximum depth nesting:' as test_description;
SELECT pg_hier($$
kingdoms (
    where {
        kingdom_id: { _eq: 1 }
    }
) {
    scientific_name,
    common_name,
    phyla {
        scientific_name,
        common_name,
        classes {
            scientific_name,
            common_name,
            orders {
                scientific_name,
                common_name,
                families {
                    scientific_name,
                    common_name,
                    genera {
                        scientific_name,
                        common_name,
                        species {
                            scientific_name,
                            common_name,
                            species_id
                        }
                    }
                }
            }
        }
    }
}
$$) as max_depth_test;

-- Test 14: Test with special characters and escaping
SELECT 'Testing special characters:' as test_description;
SELECT pg_hier_parse($$
species (
    where {
        common_name: { _like: '%''s %' }
    }
) {
    scientific_name,
    common_name,
    description
}
$$) as special_chars_test;

-- Test 15: Test with boolean logic combinations
SELECT 'Testing complex boolean logic:' as test_description;
SELECT pg_hier_parse($$
families (
    where {
        _or {
            _and {
                scientific_name: { _like: 'Fel%' }
                family_id: { _lt: 10 }
            }
            _and {
                scientific_name: { _like: 'Can%' }
                family_id: { _gt: 1 }
            }
        }
    }
) {
    scientific_name,
    common_name,
    family_id
}
$$) as boolean_logic_test;

-- Test 16: Test pagination/limiting (if supported)
SELECT 'Testing limit/offset functionality:' as test_description;
SELECT pg_hier_parse($$
species (
    limit: 5,
    offset: 10,
    order_by: { scientific_name: asc }
) {
    scientific_name,
    common_name,
    species_id
}
$$) as pagination_test;

-- Test 17: Test ordering (if supported)
SELECT 'Testing ordering functionality:' as test_description;
SELECT pg_hier_parse($$
genera (
    order_by: { 
        scientific_name: desc,
        genus_id: asc 
    }
) {
    genus_id,
    scientific_name,
    common_name
}
$$) as ordering_test;

-- Test 18: Test with NULL checks
SELECT 'Testing NULL value handling:' as test_description;
SELECT pg_hier_parse($$
species (
    where {
        _and {
            description: { _is_not_null: true }
            common_name: { _is_null: false }
        }
    }
) {
    scientific_name,
    common_name,
    description
}
$$) as null_checks_test;

-- Test 19: Test join functionality
SELECT 'Testing join generation:' as test_description;
SELECT pg_hier_join('kingdoms', 'species') as join_test;

-- Test 20: Test format functionality
SELECT 'Testing format functionality:' as test_description;
SELECT pg_hier_format('SELECT scientific_name, common_name FROM kingdoms LIMIT 3') as format_test;

-- Performance test with moderate complexity
SELECT 'Performance test - moderate complexity:' as test_description;
\timing on
SELECT pg_hier($$
orders {
    scientific_name,
    common_name,
    families {
        scientific_name,
        common_name,
        genera {
            scientific_name,
            common_name,
            species {
                scientific_name,
                common_name
            }
        }
    }
}
$$) as performance_test;
\timing off

-- Clean up extension
DROP EXTENSION pg_hier CASCADE;
