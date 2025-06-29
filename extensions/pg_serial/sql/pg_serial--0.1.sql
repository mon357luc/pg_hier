CREATE FUNCTION compile_check()
RETURNS text
AS 'MODULE_PATHNAME', 'compile_check'
LANGUAGE C STRICT;

/* aggregate wrapper */
CREATE FUNCTION hierify_sfunc(internal, VARIADIC "any")
RETURNS internal
AS 'MODULE_PATHNAME', 'hierify_sfunc'
LANGUAGE C IMMUTABLE;

CREATE FUNCTION hierify_ffunc(internal)
RETURNS text
AS 'MODULE_PATHNAME', 'hierify_ffunc'
LANGUAGE C IMMUTABLE;

CREATE AGGREGATE hierify(VARIADIC "any")
(
    SFUNC     = hierify_sfunc,
    STYPE     = internal,
    FINALFUNC = hierify_ffunc
);

/* header helper */
CREATE FUNCTION hier_header(VARIADIC "any")
RETURNS text
AS 'MODULE_PATHNAME', 'hier_header'
LANGUAGE C STRICT;

CREATE FUNCTION hierify_query(TEXT)
RETURNS text
AS 'MODULE_PATHNAME', 'hierify_query'
LANGUAGE C STRICT;