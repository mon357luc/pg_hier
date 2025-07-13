ALTER USER postgres
SET search_path TO public,
    taxon;
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
SELECT *
FROM pg_hier_header;
SELECT *
FROM pg_hier_detail;
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
--order_id = 1
SET search_path TO public,
    taxon;
SELECT pg_hier (
        $$ orders { scientific_name,
        common_name,
        families { scientific_name,
        common_name,
        genera { scientific_name,
        common_name,
        genus_id },
        species { scientific_name,
        common_name,
        species_id } },
        order_id } $$
    );
--order_id = 1