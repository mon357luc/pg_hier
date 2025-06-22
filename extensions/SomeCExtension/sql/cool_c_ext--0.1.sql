CREATE FUNCTION hello_world(input_array text[]) 
RETURNS text
AS 'MODULE_PATHNAME', 'hello_world'
LANGUAGE C STRICT;


CREATE FUNCTION subq_csv(record)
RETURNS text
AS 'MODULE_PATHNAME', 'subq_csv'
LANGUAGE C STRICT;

CREATE FUNCTION pg_hier_sfunc(INTERNAL, VARIADIC "any")
RETURNS INTERNAL
AS 'MODULE_PATHNAME', 'pg_hier_sfunc'
LANGUAGE C IMMUTABLE;

CREATE FUNCTION pg_hier_ffunc(INTERNAL)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_hier_ffunc'
LANGUAGE C IMMUTABLE;

CREATE AGGREGATE pg_hier (VARIADIC "any") (
    SFUNC = pg_hier_sfunc,
    STYPE = INTERNAL,
    FINALFUNC = pg_hier_ffunc
);
