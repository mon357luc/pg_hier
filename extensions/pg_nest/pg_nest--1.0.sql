CREATE FUNCTION pg_nest_many(
  query text,
  fk text,
  parent_id integer,
  typename text,
  type_sample anyelement
) RETURNS anyarray
AS 'pg_nest', 'pg_nest_many'
LANGUAGE C IMMUTABLE;
