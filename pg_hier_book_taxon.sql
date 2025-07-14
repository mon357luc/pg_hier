SELECT pg_hier_create_hier (
        ARRAY [
            'kingdoms',
            'phyla',
            'classes',
            'orders',
            'families',
            'genera',
            'species'
        ],
        ARRAY [
            NULL::TEXT,
            'kingdom_id',
            'plylum_id',
            'class_id',
            'order_id',
            'family_id',
            'genus_id'
        ],
        ARRAY [
            NULL::TEXT,
            'kingdom_id',
            'plylum_id',
            'class_id',
            'order_id',
            'family_id',
            'genus_id'
        ]
    );
SET search_path TO public,
    taxon;
SELECT pg_hier_parse (
        $$ orders [order_id = 1] { scientific_name,
        common_name,
        families { scientific_name,
        common_name,
        genera { scientific_name,
        common_name,
        genus_id },
        species { scientific_name,
        common_name,
        species_id },
        order_id } } $$
    );