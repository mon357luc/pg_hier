CREATE FUNCTION hello_world(input_array text[]) 
RETURNS text
AS 'MODULE_PATHNAME', 'hello_world'
LANGUAGE C STRICT;