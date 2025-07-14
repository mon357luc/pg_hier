-- Corrected pg_hier_book_taxon.sql
-- This file fixes the typo in the original and provides proper taxon hierarchy setup

\echo 'Setting up corrected taxonomic hierarchy for pg_hier...'

-- Ensure taxon schema exists
CREATE SCHEMA IF NOT EXISTS taxon;
SET search_path TO taxon, public;

-- Check if taxon tables exist, if not, create them
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'taxon' AND table_name = 'kingdoms') THEN
        RAISE NOTICE 'Taxon tables not found, creating them...';
        \i ../../taxon_schema.sql
    ELSE
        RAISE NOTICE 'Taxon tables already exist';
    END IF;
END $$;

-- Clean up any existing pg_hier taxonomy hierarchy
DELETE FROM pg_hier_detail WHERE hierarchy_id IN (
    SELECT id FROM pg_hier_header WHERE table_path LIKE '%kingdoms%'
);
DELETE FROM pg_hier_header WHERE table_path LIKE '%kingdoms%';

\echo 'Creating corrected taxonomic hierarchy...'

-- Create the taxonomic hierarchy (fixed typo from original: plylum_id → phylum_id)
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
        'phylum_id',  -- FIXED: was 'plylum_id' in original
        'class_id',
        'order_id',
        'family_id',
        'genus_id'
    ],
    ARRAY[
        NULL::TEXT,
        'kingdom_id',
        'phylum_id',  -- FIXED: was 'plylum_id' in original
        'class_id',
        'order_id',
        'family_id',
        'genus_id'
    ]
);

-- Verify hierarchy creation
SELECT 'Taxonomic hierarchy created successfully:' as status, table_path FROM pg_hier_header WHERE table_path LIKE '%kingdoms%';

\echo 'Testing corrected hierarchy with sample queries...'

-- Test the corrected hierarchy with the original query concept (fixed syntax)
SELECT 'Carnivora order with families and genera:' as test, pg_hier('
    orders( where { 
        order_id { _eq: 1 }
    } ) { 
        scientific_name,
        common_name,
        families { 
            scientific_name,
            common_name,
            genera { 
                scientific_name,
                common_name,
                genus_id
            }
        }
    }
') as result;

-- More comprehensive test - get all mammals
SELECT 'All mammalian families:' as test, pg_hier('
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
') as mammal_families;

-- Test parsing functionality
SELECT 'Parse test result:' as test, pg_hier_parse('
    kingdoms {
        scientific_name,
        common_name,
        phyla {
            scientific_name,
            classes {
                scientific_name,
                orders {
                    scientific_name,
                    families {
                        scientific_name,
                        genera {
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
') as parse_result;

-- Test join functionality between taxonomic levels
SELECT 'Join test result:' as test, pg_hier_join('kingdoms', 'species') as join_result;

-- Reset search path
SET search_path TO public;

\echo ''
\echo 'Corrected taxonomic hierarchy setup completed!'
\echo 'The original typo "plylum_id" has been fixed to "phylum_id"'
\echo 'All taxonomic queries should now work properly.'
\echo ''
