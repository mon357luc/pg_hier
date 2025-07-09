/**************************************
 * Define scehma and set search path
 **************************************/
-- CREATE SCHEMA hier;
-- SET search_path TO hier, public;

/**************************************
 * Define necessary tables
 **************************************/
CREATE TABLE IF NOT EXISTS pg_hier_header (
    id SERIAL PRIMARY KEY,
    table_path TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS pg_hier_detail (
    id SERIAL PRIMARY KEY,
    hierarchy_id INT NOT NULL REFERENCES pg_hier_header(id) ON DELETE CASCADE, 
    name TEXT NOT NULL,
    parent_id INT REFERENCES pg_hier_detail(id),
    parent_name TEXT, 
    child_id INT REFERENCES pg_hier_detail(id),
    level INT,
    parent_key TEXT[],
    child_key TEXT[]
);

 /**************************************
 * Table indexes
 **************************************/
CREATE INDEX IF NOT EXISTS idx_pg_hier_detail_hierarchy ON pg_hier_detail(hierarchy_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_detail_hierarchy ON pg_hier_detail(hierarchy_id, name);
CREATE INDEX IF NOT EXISTS idx_pg_hier_detail_parent ON pg_hier_detail(parent_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_detail_child ON pg_hier_detail(child_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_pg_hier_detail_unique ON pg_hier_detail(parent_id, child_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_detail_name ON pg_hier_detail(name);

/**************************************
 * Define C source code functions
 **************************************/
CREATE FUNCTION pg_hier(text) 
RETURNS JSONB
AS 'MODULE_PATHNAME', 'pg_hier'
LANGUAGE C STRICT;

CREATE FUNCTION pg_hier_parse(text) 
RETURNS text
AS 'MODULE_PATHNAME', 'pg_hier_parse'
LANGUAGE C STRICT;

CREATE FUNCTION pg_hier_join(TEXT, TEXT)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_hier_join'
LANGUAGE C STRICT;

CREATE FUNCTION pg_hier_format(TEXT)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_hier_format'
LANGUAGE C STRICT;

/**************************************
 * Define SQL source code functions
 **************************************/
CREATE OR REPLACE FUNCTION pg_hier_create_hier(
    path_names text[],
    parent_path_keys text[],
    child_path_keys text[]
)
RETURNS VOID AS
$$
DECLARE
    i INT;
    hier_id INT;
    curr_id INT;
    v_parent_id INT;
    v_child_id INT;
    table_path_string TEXT;
    re_raise_exception BOOLEAN := TRUE; 
BEGIN
    BEGIN  
        IF array_length(path_names, 1) IS NULL OR array_length(path_names, 1) < 2 THEN
            RAISE EXCEPTION 'Path names must contain at least two elements';
        END IF;

        IF array_length(path_names, 1) != array_length(parent_path_keys, 1) THEN
            RAISE EXCEPTION 'parent_path_keys must have the same number of elements as path_names';
        END IF;

        IF array_length(path_names, 1) != array_length(child_path_keys, 1) THEN
            RAISE EXCEPTION 'child_path_keys must have the same number of elements as path_names';
        END IF;

        SELECT COALESCE(max(id) + 1, 1) INTO hier_id FROM pg_hier_header;

        table_path_string := array_to_string(path_names, '.');

        IF EXISTS (SELECT 1 FROM pg_hier_header WHERE table_path = table_path_string) THEN
            RAISE NOTICE 'Hierarchy % already exists. Skipping creation.', table_path_string;
            RETURN;
        END IF;

        INSERT INTO pg_hier_header (id, table_path)
        VALUES (hier_id, table_path_string);

        FOR i IN 1 .. array_length(path_names, 1) LOOP
            INSERT INTO pg_hier_detail (hierarchy_id, level, name, parent_key, child_key)
            VALUES (
                hier_id,
                i,
                path_names[i],
                CASE WHEN i = 1 THEN '{}'::text[] ELSE string_to_array(parent_path_keys[i], ':') END, 
                CASE WHEN i = 1 THEN '{}'::text[] ELSE string_to_array(child_path_keys[i], ':') END 
            );
        END LOOP;

        RAISE NOTICE 'Hierarchy % created with ID %', table_path_string, hier_id;

        FOR i IN 1 .. array_length(path_names, 1) LOOP
            SELECT id INTO curr_id FROM pg_hier_detail WHERE hierarchy_id = hier_id AND name = path_names[i];
            IF NOT FOUND THEN
                RAISE EXCEPTION 'Could not find pg_hier_detail record for level % and name %', i, path_names[i];
            END IF;

            IF i > 1 THEN
                SELECT id INTO v_parent_id FROM pg_hier_detail WHERE hierarchy_id = hier_id AND name = path_names[i-1];
                IF NOT FOUND THEN
                   RAISE EXCEPTION 'Could not find pg_hier_detail record for parent level % and name %', i-1, path_names[i-1];
                END IF;
                UPDATE pg_hier_detail 
                SET 
                    parent_id = v_parent_id, 
                    parent_name = (SELECT name FROM pg_hier_detail WHERE id = v_parent_id)
                WHERE id = curr_id;
            END IF;

            IF i < array_length(path_names, 1) THEN
                SELECT id INTO v_child_id FROM pg_hier_detail WHERE hierarchy_id = hier_id AND name = path_names[i+1];
                IF NOT FOUND THEN
                    RAISE EXCEPTION 'Could not find pg_hier_detail record for child level % and name %', i+1, path_names[i+1];
                END IF;

                UPDATE pg_hier_detail SET child_id = v_child_id WHERE id = curr_id;
            END IF;
        END LOOP;

    EXCEPTION
        WHEN OTHERS THEN
            IF re_raise_exception THEN
                RAISE; 
            ELSE
               RAISE NOTICE 'Exception caught, but not re-raised.';
            END IF;
    END;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION quote_ident(input text) 
RETURNS text AS $$
BEGIN
  RETURN quote_ident(input);
END;
$$ LANGUAGE plpgsql IMMUTABLE;

CREATE OR REPLACE FUNCTION pg_hier_find_hier(
    table_names TEXT[]
)
RETURNS INT AS $$
DECLARE
    hier_id INT;
BEGIN
    IF array_length(table_names, 1) IS NULL OR array_length(table_names, 1) < 2 THEN
        RAISE EXCEPTION 'Table names must contain at least two elements';
    END IF;
    SELECT id INTO hier_id FROM pg_hier_header
    WHERE
        (SELECT bool_and(table_path LIKE '%' || table_names[i] || '%')
         FROM generate_subscripts(table_names, 1) AS i)
    LIMIT 1;

    RETURN hier_id;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION pg_hier_make_key_step(parent_keys text[], child_keys text[])
RETURNS text[] AS $$
BEGIN
    IF parent_keys IS NULL OR array_length(parent_keys, 1) IS NULL THEN
        RETURN '{[]}'::text[];
    END IF;
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
