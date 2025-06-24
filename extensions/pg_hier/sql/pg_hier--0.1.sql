CREATE TABLE IF NOT EXISTS pg_hier_table (
    id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES pg_hier_table(id),
    child_id INT REFERENCES pg_hier_table(id),
    level INT,
    name TEXT,
    parent_key TEXT,
    child_key TEXT
);

-- Create an index on the parent_id and child_id columns for faster lookups
CREATE INDEX IF NOT EXISTS idx_pg_hier_table_parent ON pg_hier_table(parent_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_table_child ON pg_hier_table(child_id);
-- Create a unique index to prevent duplicate parent-child relationships
CREATE UNIQUE INDEX IF NOT EXISTS idx_pg_hier_table_unique ON pg_hier_table(parent_id, child_id);

-- Create a function to insert a new hierarchy entry
CREATE OR REPLACE FUNCTION insert_hierarchy_entry(
    p_parent_id INT,
    p_child_id INT,
    p_level INT,
    p_name TEXT,
    p_parent_key TEXT,
    p_child_key TEXT
) RETURNS VOID AS $$
BEGIN
    INSERT INTO pg_hier_table (parent_id, child_id, level, name, parent_key, child_key)
    VALUES (p_parent_id, p_child_id, p_level, p_name, p_parent_key, p_child_key);
END;
$$ LANGUAGE plpgsql;

-- Create a function to retrieve the hierarchy for a given parent_id
CREATE OR REPLACE FUNCTION get_hierarchy_by_parent(p_parent_id INT)
RETURNS TABLE (
    id INT,
    parent_id INT,
    child_id INT,
    level INT,
    name TEXT,
    parent_key TEXT,
    child_key TEXT
) AS $$
BEGIN
    RETURN QUERY
    SELECT id, parent_id, child_id, level, name, parent_key, child_key
    FROM pg_hier_table
    WHERE parent_id = p_parent_id;
END;

-- Create a function to retrieve the hierarchy for a given child_id
CREATE OR REPLACE FUNCTION get_hierarchy_by_child(p_child_id INT)
RETURNS TABLE (
    id INT,
    parent_id INT,
    child_id INT,
    level INT,
    name TEXT,
    parent_key TEXT,
    child_key TEXT
) AS $$
BEGIN
    RETURN QUERY
    SELECT id, parent_id, child_id, level, name, parent_key, child_key
    FROM pg_hier_table
    WHERE child_id = p_child_id;
END;
$$ LANGUAGE plpgsql;

-- Create a function to delete a hierarchy entry
CREATE OR REPLACE FUNCTION delete_hierarchy_entry(p_id INT)
RETURNS VOID AS $$
BEGIN
    DELETE FROM pg_hier_table
    WHERE id = p_id;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION pg_hier_join(v_parent_name regclass, v_child_name regclass)
RETURNS TEXT AS
$$
DECLARE
    join_sql text;
    path_names text[];
    path_keys text[];
    i int;
    col_list text;
    header text;
    json_result text;
BEGIN
    WITH RECURSIVE path AS (
        SELECT id, name, parent_id, child_id, parent_key, child_key,
                ARRAY[name] AS name_path,
                ARRAY[parent_key || ':' || child_key] AS key_path
        FROM pg_hier_table
        WHERE id = (SELECT id FROM pg_hier_table WHERE name = v_parent_name::text)
        UNION ALL
        SELECT h.id, h.name, h.parent_id, h.child_id, h.parent_key, h.child_key,
                p.name_path || h.name,
                p.key_path || (h.parent_key || ':' || h.child_key)
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

    RAISE NOTICE 'col_list: %', col_list;

    join_sql := 'SELECT ' || col_list || ' FROM ' || quote_ident(path_names[1]);
    FOR i IN 2 .. array_length(path_names, 1) LOOP
        join_sql := join_sql || ' JOIN ' || quote_ident(path_names[i]) ||
            ' ON ' || quote_ident(path_names[i-1]) || '.' || split_part(path_keys[i], ':', 1) ||
            ' = ' || quote_ident(path_names[i]) || '.' || split_part(path_keys[i], ':', 2);
    END LOOP;

    RAISE NOTICE 'join_sql: %', join_sql;

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

    RAISE NOTICE 'header: %', header;

    -- Build the CSV rows
    EXECUTE format(
        'SELECT json_agg(t) FROM (%s) t',
        join_sql
    ) INTO json_result;

    RETURN json_result;
END;
$$ LANGUAGE plpgsql;