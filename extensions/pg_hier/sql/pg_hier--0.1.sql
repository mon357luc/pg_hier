CREATE TABLE IF NOT EXISTS pg_hier (
    id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES pg_hier(id),
    child_id INT REFERENCES pg_hier(id),
    level INT,
    name TEXT,
    parent_key TEXT,
    child_key TEXT
);

-- Create an index on the parent_id and child_id columns for faster lookups
CREATE INDEX IF NOT EXISTS idx_pg_hier_parent ON pg_hier(parent_id);
CREATE INDEX IF NOT EXISTS idx_pg_hier_child ON pg_hier(child_id);
-- Create a unique index to prevent duplicate parent-child relationships
CREATE UNIQUE INDEX IF NOT EXISTS idx_pg_hier_unique ON pg_hier(parent_id, child_id);

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
    INSERT INTO pg_hier (parent_id, child_id, level, name, parent_key, child_key)
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
    FROM pg_hier
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
    FROM pg_hier
    WHERE child_id = p_child_id;
END;
$$ LANGUAGE plpgsql;

-- Create a function to delete a hierarchy entry
CREATE OR REPLACE FUNCTION delete_hierarchy_entry(p_id INT)
RETURNS VOID AS $$
BEGIN
    DELETE FROM pg_hier
    WHERE id = p_id;
END;
$$ LANGUAGE plpgsql;
