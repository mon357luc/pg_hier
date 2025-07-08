SELECT * FROM pg_hier_join ('item', 'smartphone');

SELECT pg_hier_create_hier (
        ARRAY[
            'item', 'product', 'electronic_product', 'smartphone'
        ], ARRAY[
            NULL::TEXT, 'item_id', 'product_id', 'product_id:electronic_id'
        ], ARRAY[
            NULL::TEXT, 'item_id', 'product_id', 'product_id:electronic_id'
        ]
    )


SELECT pg_hier (
        '
    item { 
        name, 
        product { 
            product_id, 
            smartphone { 
                os,
                storage_gb,
                has_5g
            }, 
            item_id
        }, 
        description
    }
'
    );

SELECT pg_hier_parse (
        '
    item { 
        name, 
        product { 
            product_id, 
            smartphone { 
                os,
                storage_gb,
                has_5g
            }, 
            item_id
        }, 
        description
    }
'
    );

SELECT *
FROM pg_hier_detail
WHERE
    name LIKE '%product%'
    OR parent_id IN (
        SELECT id
        FROM pg_hier_detail
        WHERE
            name LIKE '%product%'
    );

SELECT * FROM pg_hier_detail;

TRUNCATE TABLE pg_hier_detail RESTART IDENTITY;

DELETE FROM pg_hier_header;

SELECT pg_hier_find_hier ( ARRAY[ 'product', 'smartphone', 'item' ] );

SELECT jsonb_build_object(
    'item_id', array_agg(item_id),
    'nested', jsonb_build_object(
        'name', array_agg(name),
        'description', array_agg(description)
    ))
FROM item

SELECT smartphone.os, jsonb_build_object(
        'item', json_build_object(
            'name', array_agg(item.name), 'product', json_build_object(
                'product_id', array_agg(product.product_id), 'smartphone', json_build_object(
                    'os', array_agg(smartphone.os), 'storage_gb', array_agg(smartphone.storage_gb), 'has_5g', array_agg(smartphone.has_5g)
                ), 'item_id', array_agg(product.item_id)
            ), 'description', array_agg(item.description)
        )
    )
FROM
    item
    JOIN product ON item.item_id = product.item_id
    JOIN electronic_product ON product.product_id = electronic_product.product_id
    JOIN smartphone ON electronic_product.electronic_id = smartphone.electronic_id
GROUP BY smartphone.os