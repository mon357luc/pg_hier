CREATE FUNCTION hello_world(input_array text[]) 
RETURNS text
AS 'MODULE_PATHNAME', 'hello_world'
LANGUAGE C STRICT;


CREATE FUNCTION subq_csv(record)
RETURNS text
AS 'MODULE_PATHNAME', 'subq_csv'
LANGUAGE C STRICT;
