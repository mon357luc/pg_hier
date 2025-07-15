-- Real-world usage scenarios for pg_hier functions

-- Create the extension
CREATE EXTENSION IF NOT EXISTS pg_hier;

-- Set search path
SET search_path TO taxon, public;

-- Set up the hierarchical structure
SELECT pg_hier_create_hier (
    ARRAY ['kingdoms','phyla','classes','orders','families','genera','species'],
    ARRAY [NULL::TEXT,'kingdom_id','phylum_id','class_id','order_id','family_id','genus_id'],
    ARRAY [NULL::TEXT,'kingdom_id','phylum_id','class_id','order_id','family_id','genus_id']
);

-- Scenario 1: Biodiversity Research - Find all species in Felidae family
SELECT 'Scenario 1: Biodiversity Research - Felidae family analysis' as scenario;
SELECT pg_hier($$
families (
    where {
        scientific_name: { _eq: 'Felidae' }
    }
) {
    scientific_name,
    common_name,
    description,
    genera {
        scientific_name,
        common_name,
        species {
            scientific_name,
            common_name,
            description,
            species_id
        }
    }
}
$$) as felidae_analysis;

-- Scenario 2: Conservation Biology - Endangered species tracking
SELECT 'Scenario 2: Conservation Biology - Mammalian species inventory' as scenario;
SELECT pg_hier($$
classes (
    where {
        scientific_name: { _eq: 'Mammalia' }
    }
) {
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
$$) as mammalian_inventory;

-- Scenario 3: Educational Tool - Explore a specific order (Carnivora)
SELECT 'Scenario 3: Educational Tool - Carnivora exploration' as scenario;
SELECT pg_hier($$
orders (
    where {
        scientific_name: { _eq: 'Carnivora' }
    }
) {
    scientific_name,
    common_name,
    description,
    families (
        where {
            _or {
                scientific_name: { _like: '%idae' }
                common_name: { _is_not_null: true }
            }
        }
    ) {
        scientific_name,
        common_name,
        description,
        genera {
            scientific_name,
            common_name,
            species (
                limit: 3
            ) {
                scientific_name,
                common_name,
                description
            }
        }
    }
}
$$) as carnivora_exploration;

-- Scenario 4: Taxonomic Validation - Check hierarchy consistency
SELECT 'Scenario 4: Taxonomic Validation - Hierarchy consistency check' as scenario;
SELECT pg_hier_join('kingdoms', 'species') as full_hierarchy_join;

-- Scenario 5: Field Guide Application - Species in a geographic region
SELECT 'Scenario 5: Field Guide - Cat species overview' as scenario;
SELECT pg_hier($$
genera (
    where {
        _or {
            scientific_name: { _eq: 'Felis' }
            scientific_name: { _eq: 'Panthera' }
        }
    }
) {
    scientific_name,
    common_name,
    genus_id,
    species {
        scientific_name,
        common_name,
        description,
        species_id
    }
}
$$) as cat_species_guide;

-- Scenario 6: Research Database Query - Multi-kingdom comparison
SELECT 'Scenario 6: Multi-kingdom comparison study' as scenario;
SELECT pg_hier($$
kingdoms (
    where {
        _or {
            scientific_name: { _eq: 'Animalia' }
            scientific_name: { _eq: 'Plantae' }
            scientific_name: { _eq: 'Fungi' }
        }
    }
) {
    scientific_name,
    common_name,
    description,
    phyla (
        limit: 2
    ) {
        scientific_name,
        common_name,
        classes (
            limit: 2
        ) {
            scientific_name,
            common_name,
            orders (
                limit: 1
            ) {
                scientific_name,
                common_name
            }
        }
    }
}
$$) as multi_kingdom_study;

-- Scenario 7: API Backend - Species search by common name pattern
SELECT 'Scenario 7: API Backend - Species search functionality' as scenario;
SELECT pg_hier($$
species (
    where {
        _or {
            common_name: { _ilike: '%cat%' }
            common_name: { _ilike: '%dog%' }
            common_name: { _ilike: '%mouse%' }
        }
    }
) {
    scientific_name,
    common_name,
    description,
    species_id
}
$$) as species_search_api;

-- Scenario 8: Data Migration Tool - Export hierarchical data
SELECT 'Scenario 8: Data Migration - Hierarchical export format' as scenario;
SELECT pg_hier_format($$
SELECT 
    k.scientific_name as kingdom,
    p.scientific_name as phylum,
    c.scientific_name as class,
    o.scientific_name as order_name,
    f.scientific_name as family,
    g.scientific_name as genus,
    s.scientific_name as species
FROM kingdoms k
JOIN phyla p ON k.kingdom_id = p.kingdom_id
JOIN classes c ON p.phylum_id = c.phylum_id
JOIN orders o ON c.class_id = o.class_id
JOIN families f ON o.order_id = f.order_id
JOIN genera g ON f.family_id = g.family_id
JOIN species s ON g.genus_id = s.genus_id
WHERE k.scientific_name = 'Animalia'
LIMIT 10
$$) as migration_export;

-- Scenario 9: Phylogenetic Analysis - Trace evolutionary lineage
SELECT 'Scenario 9: Phylogenetic Analysis - Human lineage' as scenario;
SELECT pg_hier($$
species (
    where {
        scientific_name: { _eq: 'Homo sapiens' }
    }
) {
    scientific_name,
    common_name,
    species_id
}
$$) as human_lineage;

-- Get the full lineage using join
SELECT pg_hier_join('kingdoms', 'species') as full_lineage_structure;

-- Scenario 10: Inventory Management - Count species by taxonomic level
SELECT 'Scenario 10: Inventory Management - Species count by family' as scenario;
SELECT pg_hier_format($$
SELECT 
    f.scientific_name as family_name,
    f.common_name as family_common_name,
    COUNT(s.species_id) as species_count,
    COUNT(g.genus_id) as genus_count
FROM families f
LEFT JOIN genera g ON f.family_id = g.family_id
LEFT JOIN species s ON g.genus_id = s.genus_id
GROUP BY f.family_id, f.scientific_name, f.common_name
ORDER BY species_count DESC
LIMIT 10
$$) as inventory_counts;

-- Scenario 11: Bioinformatics Pipeline - Batch species processing
SELECT 'Scenario 11: Bioinformatics Pipeline - Batch processing setup' as scenario;
SELECT pg_hier_parse($$
families {
    family_id,
    scientific_name,
    common_name,
    genera {
        genus_id,
        scientific_name,
        common_name,
        species {
            species_id,
            scientific_name,
            common_name,
            description
        }
    }
}
$$) as batch_processing_structure;

-- Scenario 12: Mobile App Backend - Simplified taxonomic tree
SELECT 'Scenario 12: Mobile App - Simplified taxonomic browsing' as scenario;
SELECT pg_hier($$
kingdoms {
    scientific_name,
    common_name,
    phyla (
        limit: 3
    ) {
        scientific_name,
        common_name,
        classes (
            limit: 2
        ) {
            scientific_name,
            common_name
        }
    }
}
$$) as mobile_app_tree;

-- Scenario 13: Scientific Publication - Detailed family analysis
SELECT 'Scenario 13: Scientific Publication - Detailed Felidae analysis' as scenario;
SELECT pg_hier($$
families (
    where {
        scientific_name: { _eq: 'Felidae' }
    }
) {
    family_id,
    scientific_name,
    common_name,
    description,
    genera {
        genus_id,
        scientific_name,
        common_name,
        description,
        species {
            species_id,
            scientific_name,
            common_name,
            description
        }
    }
}
$$) as detailed_felidae_analysis;

-- Scenario 14: Citizen Science - Public species identification
SELECT 'Scenario 14: Citizen Science - Common species for identification' as scenario;
SELECT pg_hier($$
species (
    where {
        _and {
            common_name: { _is_not_null: true }
            description: { _is_not_null: true }
        }
    }
    limit: 20
) {
    scientific_name,
    common_name,
    description,
    species_id
}
$$) as citizen_science_species;

-- Scenario 15: Ecosystem Modeling - Predator-prey relationships
SELECT 'Scenario 15: Ecosystem Modeling - Carnivore species for modeling' as scenario;
SELECT pg_hier($$
orders (
    where {
        scientific_name: { _eq: 'Carnivora' }
    }
) {
    order_id,
    scientific_name,
    common_name,
    families {
        family_id,
        scientific_name,
        common_name,
        genera {
            genus_id,
            scientific_name,
            species {
                species_id,
                scientific_name,
                common_name
            }
        }
    }
}
$$) as ecosystem_carnivores;

-- Performance test for real-world scale
SELECT 'Performance Test: Real-world scale query' as scenario;
\timing on
SELECT pg_hier($$
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
                    common_name
                }
            }
        }
    }
}
$$) as performance_test;
\timing off

-- Validate data integrity
SELECT 'Data Integrity Check: Orphaned records' as scenario;
SELECT pg_hier_format($$
SELECT 
    'Orphaned phyla' as check_type,
    COUNT(*) as count
FROM phyla p
LEFT JOIN kingdoms k ON p.kingdom_id = k.kingdom_id
WHERE k.kingdom_id IS NULL
UNION ALL
SELECT 
    'Orphaned classes' as check_type,
    COUNT(*) as count
FROM classes c
LEFT JOIN phyla p ON c.phylum_id = p.phylum_id
WHERE p.phylum_id IS NULL
$$) as data_integrity_check;

-- Clean up
DROP EXTENSION pg_hier CASCADE;
