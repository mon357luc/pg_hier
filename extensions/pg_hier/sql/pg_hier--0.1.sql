CREATE TABLE IF NOT EXISTS pg_hier_table (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    parent_id INT REFERENCES pg_hier_table(id),
    parent_name TEXT, 
    child_id INT REFERENCES pg_hier_table(id),
    level INT,
    parent_key TEXT[],
    child_key TEXT[]
);

CREATE INDEX IF NOT EXISTS idx_pg_hier_table_parent ON pg_hier_table(parent_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_table_child ON pg_hier_table(child_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_pg_hier_table_unique ON pg_hier_table(parent_id, child_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_table_name ON pg_hier_table(name);

CREATE OR REPLACE FUNCTION pg_hier_create_hier(
    path_names text[],
    parent_path_keys text[],
    child_path_keys text[]
)
RETURNS VOID AS
$$
DECLARE
    i INT;
    curr_id INT;
    v_parent_id INT;
    v_child_id INT;
BEGIN
    IF array_length(path_names, 1) IS NULL OR array_length(path_names, 1) < 2 THEN
        RAISE EXCEPTION 'Path names must contain at least two elements';
    END IF;

    IF array_length(path_names, 1) = array_length(parent_path_keys, 1) - 1 AND
       array_length(path_names, 1) = array_length(child_path_keys, 1) - 1 THEN
        child_path_keys := NULL::text || child_path_keys;
        parent_path_keys := NULL::text || parent_path_keys;
    END IF;

    FOR i IN 1 .. array_length(path_names, 1) LOOP
        INSERT INTO pg_hier_table (level, name, parent_key, child_key)
        VALUES (
            i,
            path_names[i],
            string_to_array(parent_path_keys[i], ':'),
            string_to_array(child_path_keys[i], ':')
        );
    END LOOP;
    FOR i IN 1 .. array_length(path_names, 1) LOOP
        SELECT id INTO curr_id FROM pg_hier_table WHERE name = path_names[i] LIMIT 1;

        IF i > 1 THEN
            SELECT id INTO v_parent_id FROM pg_hier_table WHERE name = path_names[i-1] LIMIT 1;
            UPDATE pg_hier_table SET parent_id = v_parent_id WHERE id = curr_id;
        END IF;

        IF i < array_length(path_names, 1) THEN
            SELECT id INTO v_child_id FROM pg_hier_table WHERE name = path_names[i+1] LIMIT 1;
            UPDATE pg_hier_table SET child_id = v_child_id WHERE id = curr_id;
        END IF;
    END LOOP;
END;
$$ LANGUAGE plpgsql;

--Helper function for pg_hier_join
CREATE OR REPLACE FUNCTION make_key_step(parent_keys text[], child_keys text[])
RETURNS text[] AS $$
BEGIN
    IF parent_keys IS NULL OR array_length(parent_keys, 1) IS NULL THEN
        RETURN '[]'::json[];
    END IF;
    RAISE NOTICE 'make_key_step: generate subscripts: %d', (SELECT COUNT(*) FROM generate_subscripts(parent_keys, 1));
    RETURN ARRAY[
        COALESCE(
            to_json(
                ARRAY(
                    SELECT parent_keys[i]::text || ':' || child_keys[i]::text
                    FROM generate_subscripts(parent_keys, 1) AS i
                )
            ), '[]'
        )
    ];
END;
$$ LANGUAGE plpgsql IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION pg_hier_join(v_parent_name regclass, v_child_name regclass)
RETURNS TEXT AS
$$
DECLARE
    join_sql text;
    path_names text[];
    path_keys text[];
    key_group text[];
    key_count int;
    i int;
    k INT;
    col_list text;
    header text;
    json_result text;
BEGIN
    WITH RECURSIVE path AS (
        SELECT 
            id, name, parent_id, child_id, parent_key, child_key,
            ARRAY[name] AS name_path,
            COALESCE(make_key_step(parent_key, child_key), '{[]}') AS key_path, 
            pg_typeof(make_key_step(parent_key, child_key)) AS ty1,
            pg_typeof('') AS ty2
        FROM pg_hier_table
        WHERE name = v_parent_name::text
        UNION ALL
        SELECT 
            h.id, h.name, h.parent_id, h.child_id, h.parent_key, h.child_key,
            p.name_path || h.name,
            p.key_path || make_key_step(h.parent_key, h.child_key),
            pg_typeof(make_key_step(h.parent_key, h.child_key)), 
            pg_typeof(p.key_path)
        FROM pg_hier_table h
        JOIN path p ON h.parent_id = p.id
    )
    SELECT name_path, key_path
    INTO path_names, path_keys
    FROM path
    WHERE name = v_child_name::text;

    IF path_names IS NULL OR array_length(path_names, 1) < 2 THEN
        RAISE EXCEPTION 'No path found from % to %', v_parent_name, v_child_name;
    END IF;

    col_list := '';
    FOR i IN 1 .. array_length(path_names, 1) LOOP
        col_list := col_list || (
            SELECT string_agg(
                quote_ident(path_names[i]) || '.' || quote_ident(attname) || '::text AS ' || quote_ident(path_names[i] || '_' || attname),
                ', '
            )
            FROM pg_attribute
            WHERE attrelid = quote_ident(path_names[i])::regclass
              AND attnum > 0 AND NOT attisdropped
        );
        IF i < array_length(path_names, 1) THEN
            col_list := col_list || ', ';
        END IF;
    END LOOP;

    -- Assume path_keys is text[][]

    RAISE NOTICE 'path_names: %', path_names;
    RAISE NOTICE 'path_keys: %', path_keys;

    FOR i IN 1 .. array_length(path_keys, 1) LOOP
        RAISE NOTICE 'path_keys[%] = %', i, path_keys[i];
    END LOOP;


    join_sql := 'SELECT ' || col_list || ' FROM ' || quote_ident(path_names[1]);
    FOR i IN 2 .. array_length(path_names, 1) LOOP
        SELECT array_agg(elem)
        INTO key_group
        FROM json_array_elements_text(path_keys[i]::json) AS elem;
        key_count := coalesce(array_length(key_group, 1), 0);
        join_sql := join_sql || ' JOIN ' || quote_ident(path_names[i]);
        IF key_count > 0 THEN
            join_sql := join_sql || ' ON ';

            FOR k IN 1 .. key_count LOOP
                IF k > 1 THEN 
                    join_sql := join_sql || ' AND '; 
                END IF;

                join_sql := join_sql ||
                    quote_ident(path_names[i-1]) || '.' || quote_ident(split_part(key_group[k], ':', 1)) ||
                    ' = ' ||
                    quote_ident(path_names[i]) || '.' || quote_ident(split_part(key_group[k], ':', 2));
            END LOOP;
        END IF;
    END LOOP;


    -- Build the CSV header
    header := '';
    FOR i IN 1 .. array_length(path_names, 1) LOOP
        header := header || (
            SELECT string_agg(
                quote_ident(path_names[i] || '_' || attname),
                ','
            )
            FROM pg_attribute
            WHERE attrelid = quote_ident(path_names[i])::regclass
              AND attnum > 0 AND NOT attisdropped
        );
        IF i < array_length(path_names, 1) THEN
            header := header || ',';
        END IF;
    END LOOP;

    RAISE NOTICE 'Join SQL: %', join_sql;

    -- Build the CSV rows
    EXECUTE format(
        'SELECT json_agg(t) FROM (%s) t',
        join_sql
    ) INTO json_result;

    RETURN json_result;
END;
$$ LANGUAGE plpgsql;