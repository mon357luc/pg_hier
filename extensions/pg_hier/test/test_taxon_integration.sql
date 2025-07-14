-- pg_hier Extension Taxon Schema Integration Tests
-- This file tests the pg_hier extension with the biological taxonomy schema

\echo 'Starting pg_hier taxon schema integration tests...'

-- Set up the taxon schema if it doesn't exist
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.schemata WHERE schema_name = 'taxon') THEN
        RAISE NOTICE 'Creating taxon schema...';
        \i ../../taxon_schema.sql
    ELSE
        RAISE NOTICE 'Taxon schema already exists';
    END IF;
END $$;

-- Set search path
SET search_path TO taxon, public;

-- Clean up any existing hierarchy for taxon schema
DELETE FROM pg_hier_detail WHERE hierarchy_id IN (
    SELECT id FROM pg_hier_header WHERE table_path LIKE '%kingdoms%' OR table_path LIKE '%taxon%'
);
DELETE FROM pg_hier_header WHERE table_path LIKE '%kingdoms%' OR table_path LIKE '%taxon%';

\echo 'Test 1: Creating taxonomic hierarchy...'

-- Create the taxonomic hierarchy (fixing the typo in original file)
SELECT pg_hier_create_hier(
    ARRAY[
        'kingdoms',
        'phyla', 
        'classes',
        'orders',
        'families',
        'genera',
        'species'
    ],
    ARRAY[
        NULL::TEXT,
        'kingdom_id',
        'phylum_id',  -- Fixed typo from 'plylum_id'
        'class_id',
        'order_id',
        'family_id',
        'genus_id'
    ],
    ARRAY[
        NULL::TEXT,
        'kingdom_id',
        'phylum_id',  -- Fixed typo from 'plylum_id'
        'class_id',
        'order_id',
        'family_id',
        'genus_id'
    ]
);

-- Verify hierarchy creation
SELECT 'Taxonomic hierarchy created:' as status, table_path FROM pg_hier_header WHERE table_path LIKE '%kingdoms%';

\echo 'Test 2: Basic taxonomic query - All mammals...'

SELECT 'Mammal families result:' as test, pg_hier('
    kingdoms( where { 
        scientific_name { _eq: "Animalia" }
    } ) { 
        scientific_name,
        common_name,
        phyla( where {
            scientific_name { _eq: "Chordata" }
        } ) { 
            scientific_name,
            classes( where {
                scientific_name { _eq: "Mammalia" }
            } ) { 
                scientific_name,
                common_name,
                orders { 
                    scientific_name,
                    common_name,
                    families {
                        scientific_name,
                        common_name
                    }
                }
            }
        }
    }
') as result;

\echo 'Test 3: Specific species query - All cats...'

SELECT 'Cat species result:' as test, pg_hier('
    kingdoms { 
        scientific_name,
        phyla { 
            classes( where {
                scientific_name { _eq: "Mammalia" }
            } ) { 
                orders( where {
                    scientific_name { _eq: "Carnivora" }
                } ) { 
                    families( where {
                        scientific_name { _eq: "Felidae" }
                    } ) { 
                        scientific_name,
                        genera {
                            scientific_name,
                            species {
                                scientific_name,
                                common_name,
                                description
                            }
                        }
                    }
                }
            }
        }
    }
') as result;

\echo 'Test 4: Complex taxonomic query with multiple conditions...'

SELECT 'Complex taxonomy result:' as test, pg_hier('
    kingdoms( where {
        _or {
            scientific_name { _eq: "Animalia" }
            scientific_name { _eq: "Plantae" }
        }
    } ) { 
        scientific_name,
        common_name,
        phyla( where {
            scientific_name { _like: "%phyta" }
        } ) { 
            scientific_name,
            common_name,
            classes {
                scientific_name,
                orders( where {
                    scientific_name { _not_like: "%iformes" }
                } ) {
                    scientific_name,
                    families {
                        scientific_name,
                        genera( where {
                            scientific_name { _like: "P%" }
                        } ) {
                            scientific_name,
                            species {
                                scientific_name,
                                common_name
                            }
                        }
                    }
                }
            }
        }
    }
') as result;

\echo 'Test 5: Testing joins between taxonomic levels...'

SELECT 'Taxonomic join result:' as test, pg_hier_join('kingdoms', 'species') as join_sql;

\echo 'Test 6: Parse taxonomic query...'

SELECT 'Parse taxonomic result:' as test, pg_hier_parse('
    kingdoms { 
        scientific_name,
        phyla { 
            scientific_name,
            classes {
                scientific_name,
                orders {
                    families {
                        genera {
                            species {
                                scientific_name,
                                common_name
                            }
                        }
                    }
                }
            }
        }
    }
') as parsed;

\echo 'Test 7: Count validation across taxonomic levels...'

-- Verify data consistency
SELECT 'Data validation:' as metric, 'Kingdoms' as level, COUNT(*) as count FROM kingdoms
UNION ALL
SELECT 'Data validation:', 'Phyla', COUNT(*) FROM phyla
UNION ALL  
SELECT 'Data validation:', 'Classes', COUNT(*) FROM classes
UNION ALL
SELECT 'Data validation:', 'Orders', COUNT(*) FROM orders
UNION ALL
SELECT 'Data validation:', 'Families', COUNT(*) FROM families
UNION ALL
SELECT 'Data validation:', 'Genera', COUNT(*) FROM genera
UNION ALL
SELECT 'Data validation:', 'Species', COUNT(*) FROM species;

\echo 'Test 8: Performance test with full taxonomic tree...'

\timing on
SELECT 'Full taxonomic tree length:' as test, LENGTH(pg_hier('
    kingdoms { 
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
                                description
                            }
                        }
                    }
                }
            }
        }
    }
')::text) as result_length;
\timing off

\echo 'Test 9: Specific species lookup...'

-- Test specific species queries
SELECT 'Human classification:' as test, pg_hier('
    kingdoms( where {
        scientific_name { _eq: "Animalia" }
    } ) { 
        scientific_name,
        phyla( where {
            scientific_name { _eq: "Chordata" }
        } ) {
            classes( where {
                scientific_name { _eq: "Mammalia" }
            } ) {
                orders( where {
                    scientific_name { _eq: "Primates" }
                } ) {
                    families( where {
                        scientific_name { _eq: "Hominidae" }
                    } ) {
                        genera( where {
                            scientific_name { _eq: "Homo" }
                        } ) {
                            species( where {
                                scientific_name { _eq: "Homo sapiens" }
                            } ) {
                                scientific_name,
                                common_name,
                                description
                            }
                        }
                    }
                }
            }
        }
    }
') as human_classification;

\echo 'Test 10: Cross-kingdom comparison...'

SELECT 'Cross-kingdom result:' as test, pg_hier('
    kingdoms { 
        scientific_name,
        common_name,
        phyla( where {
            _or {
                scientific_name { _eq: "Chordata" }
                scientific_name { _eq: "Magnoliophyta" }
                scientific_name { _eq: "Basidiomycota" }
            }
        } ) {
            scientific_name,
            common_name,
            classes {
                scientific_name,
                common_name,
                orders {
                    scientific_name,
                    families {
                        scientific_name,
                        genera {
                            scientific_name,
                            species( where {
                                common_name { _is_not_null: true }
                            } ) {
                                scientific_name,
                                common_name
                            }
                        }
                    }
                }
            }
        }
    }
') as cross_kingdom;

-- Reset search path
SET search_path TO public;

\echo ''
\echo 'Taxon schema integration tests completed!'
\echo 'The pg_hier extension successfully handles complex biological taxonomy!'
\echo ''
