#include "pg_hier.h"
#include "pg_hier_security.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pg_hier);
/**************************************
 * function pg_hier builds and
 * executes a hierarchical SQL query.
 *
 * Relies on pg_hier_parse
 * and pg_hier_join.
 *
 * CREATE FUNCTION pg_hier(text)
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier'
 * LANGUAGE C STRICT;
 ****************************/
Datum pg_hier(PG_FUNCTION_ARGS)
{
    text *input_text;
    char *input;
    StringInfoData parse_buf;
    string_array *tables = NULL;
    Datum result = (Datum)NULL;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_text = PG_GETARG_TEXT_PP(0);
    input = text_to_cstring(input_text);
    input = trim_whitespace(input);

    if (strlen(input) == 0)
        PG_RETURN_NULL();

    initStringInfo(&parse_buf);

    appendStringInfoString(&parse_buf, "SELECT jsonb_agg(jsonb_build_object(");
    parse_input(&parse_buf, input, &tables);

    if (tables == NULL)
    {
        free_string_array(tables);
        pg_hier_error_no_tables();
    }

    free_string_array(tables);
    pfree(input);

    result = pg_hier_return_one(parse_buf.data);

    pfree(parse_buf.data);

    PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(pg_hier_parse);
/**************************************
 * CREATE FUNCTION pg_hier_parse(text)
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier_parse'
 * LANGUAGE C STRICT;
 **************************************/
Datum pg_hier_parse(PG_FUNCTION_ARGS)
{
    text *input_text;
    char *input;
    StringInfoData parse_buf;
    string_array *tables = NULL;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_text = PG_GETARG_TEXT_PP(0);
    input = text_to_cstring(input_text);
    input = trim_whitespace(input);
    
    if (strlen(input) == 0)
        PG_RETURN_NULL();

    initStringInfo(&parse_buf);

    appendStringInfoString(&parse_buf, "SELECT jsonb_agg(jsonb_build_object(");
    parse_input(&parse_buf, input, &tables);

    if (tables == NULL)
    {
        pfree(input);
        pfree(parse_buf.data);
        pg_hier_error_no_tables();
    }
    free_string_array(tables);
    pfree(input);

    PG_RETURN_TEXT_P(cstring_to_text(parse_buf.data));
}

PG_FUNCTION_INFO_V1(pg_hier_join);
/**************************************
 * CREATE FUNCTION pg_hier_join(TEXT, TEXT)
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier_join'
 * LANGUAGE C STRICT;
 **************************************/
Datum pg_hier_join(PG_FUNCTION_ARGS)
{
    bool isnull;
    text *parent_name_text;
    text *child_name_text;
    char *parent_name;
    char *child_name;
    Datum *name_path_elems;
    bool *name_path_nulls;
    int name_path_count;
    Datum *key_path_elems;
    bool *key_path_nulls;
    int key_path_count;
    char *parent_table = NULL;
    char *join_table = NULL;
    char *key_json = NULL;
    char *key_array = NULL;
    char *end_array = NULL;
    int key_idx;
    char *saveptr = NULL;
    StringInfoData join_sql;
    int ret;
    const char *fetch_hier;
    HeapTuple tuple;
    TupleDesc tupdesc;
    char *right_key;
    char *pair;
    char *end_quote;
    char *colon;
    char *left_key;
    char *escaped_first_table;
    Datum name_path_datum, key_path_datum;
    ArrayType *name_path_arr, *key_path_arr;
    int name_path_len;
    Oid argtypes[2];
    Datum values[2];

    parent_name_text = PG_GETARG_TEXT_PP(0);
    child_name_text = PG_GETARG_TEXT_PP(1);

    parent_name = text_to_cstring(parent_name_text);
    child_name = text_to_cstring(child_name_text);

    // Validate table names
    VALIDATE_IDENTIFIER(parent_name, "parent table name");
    VALIDATE_IDENTIFIER(child_name, "child table name");

    initStringInfo(&join_sql);

    ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        pg_hier_error_spi_connect(ret);

    fetch_hier =
        "WITH RECURSIVE path AS ( "
        "    SELECT "
        "        id, name, parent_id, child_id, parent_key, child_key, "
        "        ARRAY[name] AS name_path, "
        "        COALESCE(pg_hier_make_key_step(parent_key, child_key), '{[]}') AS key_path, "
        "        pg_typeof(pg_hier_make_key_step(parent_key, child_key)) AS ty1, "
        "        pg_typeof('') AS ty2 "
        "    FROM pg_hier_detail "
        "    WHERE name = $1 "
        "    UNION ALL "
        "    SELECT "
        "        h.id, h.name, h.parent_id, h.child_id, h.parent_key, h.child_key, "
        "        p.name_path || h.name, "
        "        p.key_path || pg_hier_make_key_step(h.parent_key, h.child_key), "
        "        pg_typeof(pg_hier_make_key_step(h.parent_key, h.child_key)), "
        "        pg_typeof(p.key_path) "
        "    FROM pg_hier_detail h "
        "    JOIN path p ON h.parent_id = p.id "
        ") "
        "SELECT name_path, key_path "
        "FROM path "
        "WHERE name = $2;";

    argtypes[0] = TEXTOID;
    argtypes[1] = TEXTOID;
    values[0] = PointerGetDatum(parent_name_text);
    values[1] = PointerGetDatum(child_name_text);
    ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        pg_hier_error_spi_execute_with_args(ret);
    }

    if (SPI_processed == 0)
    {
        SPI_finish();
        pg_hier_error_no_path_found(parent_name, child_name);
    }

    tuple = SPI_tuptable->vals[0];
    tupdesc = SPI_tuptable->tupdesc;

    name_path_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
    key_path_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);

    name_path_arr = DatumGetArrayTypeP(name_path_datum);
    key_path_arr = DatumGetArrayTypeP(key_path_datum);

    name_path_len = ArrayGetNItems(ARR_NDIM(name_path_arr), ARR_DIMS(name_path_arr));

    if (name_path_len < 2)
    {
        SPI_finish();
        pg_hier_error_no_path_found(parent_name, child_name);
    }

    deconstruct_array(name_path_arr, TEXTOID, -1, false, 'i',
                      &name_path_elems, &name_path_nulls, &name_path_count);

    deconstruct_array(key_path_arr, TEXTOID, -1, false, 'i',
                      &key_path_elems, &key_path_nulls, &key_path_count);

    resetStringInfo(&join_sql);
    
    escaped_first_table = ESCAPE_IDENTIFIER(TextDatumGetCString(name_path_elems[0]));
    appendStringInfo(&join_sql, "FROM %s", escaped_first_table);
    pfree(escaped_first_table);

    for (int i = 1; i < name_path_count; i++)
    {
        char *escaped_join_table;
        char *escaped_parent_table;
        char *escaped_left_key;
        char *escaped_right_key;
        
        parent_table = pstrdup(TextDatumGetCString(name_path_elems[i - 1]));
        join_table = pstrdup(TextDatumGetCString(name_path_elems[i]));
        
        // Validate table names
        VALIDATE_IDENTIFIER(parent_table, "parent table in join");
        VALIDATE_IDENTIFIER(join_table, "join table in join");
        
        escaped_join_table = ESCAPE_IDENTIFIER(join_table);
        
        key_json = TextDatumGetCString(key_path_elems[i]);
        key_array = key_json;
        while (*key_array && (*key_array == '[' || *key_array == ' '))
            key_array++;

        end_array = key_array + strlen(key_array) - 1;
        while (end_array > key_array && (*end_array == ']' || *end_array == ' '))
        {
            *end_array = '\0';
            end_array--;
        }

        pair = strtok_r(key_array, ",", &saveptr);
        appendStringInfo(&join_sql, " JOIN %s ON ", escaped_join_table);
        key_idx = 0;
        while (pair)
        {
            while (*pair && (*pair == '"' || *pair == ' '))
                pair++;

            end_quote = strchr(pair, '"');
            if (end_quote)
                *end_quote = '\0';

            colon = strchr(pair, ':');
            if (!colon)
            {
                SPI_finish();
                pg_hier_error_invalid_key_format(pair);
            }
            *colon = '\0';
            left_key = pair;
            right_key = colon + 1;

            // Validate column names
            VALIDATE_IDENTIFIER(left_key, "left key in join");
            VALIDATE_IDENTIFIER(right_key, "right key in join");

            if (key_idx > 0)
                appendStringInfoString(&join_sql, " AND ");

            pg_hier_info_table_check(TextDatumGetCString(name_path_elems[i]));

            escaped_parent_table = ESCAPE_IDENTIFIER(parent_table);
            escaped_left_key = ESCAPE_IDENTIFIER(left_key);
            escaped_right_key = ESCAPE_IDENTIFIER(right_key);
            
            appendStringInfo(&join_sql, "%s.%s = %s.%s",
                             escaped_parent_table, escaped_left_key,
                             escaped_join_table, escaped_right_key);
                             
            pfree(escaped_parent_table);
            pfree(escaped_left_key);
            pfree(escaped_right_key);

            key_idx++;
            pair = strtok_r(NULL, ",", &saveptr);
        }
        
        pfree(escaped_join_table);
        pfree(parent_table);
        pfree(join_table);
        parent_table = NULL;
        join_table = NULL;
    }
    SPI_finish();
    PG_RETURN_TEXT_P(cstring_to_text(join_sql.data));
}

PG_FUNCTION_INFO_V1(pg_hier_format);

/**************************************
 * CREATE FUNCTION pg_hier_format(TEXT)
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier_format'
 * LANGUAGE C STRICT;
 **************************************/
Datum pg_hier_format(PG_FUNCTION_ARGS)
{
    text *sql;
    char *query;
    char *trimmed;
    StringInfoData buf;
    int ret;
    bool format_isnull;
    char *outstr;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    sql = PG_GETARG_TEXT_PP(0);
    query = text_to_cstring(sql);

    // Basic SQL injection prevention - reject dangerous patterns
    if (strstr(query, ";") || strstr(query, "--") || 
        pg_strcasecmp(query, "DROP") || pg_strcasecmp(query, "DELETE") ||
        pg_strcasecmp(query, "UPDATE") || pg_strcasecmp(query, "INSERT") ||
        pg_strcasecmp(query, "CREATE") || pg_strcasecmp(query, "ALTER") ||
        pg_strcasecmp(query, "TRUNCATE") || pg_strcasecmp(query, "EXECUTE"))
    {
        PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT,
            "Only SELECT statements are allowed in pg_hier_format");
    }

    // Ensure it starts with SELECT (case insensitive)
    trimmed = query;
    while (*trimmed && isspace((unsigned char)*trimmed))
        trimmed++;
    
    if (pg_strncasecmp(trimmed, "SELECT", 6) != 0)
    {
        PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT,
            "Query must start with SELECT statement");
    }

    initStringInfo(&buf);
    appendStringInfoChar(&buf, '{');

    ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        pg_hier_error_spi_connect(ret);

    // Execute with read-only transaction
    ret = SPI_execute(query, true, 0);
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        pg_hier_error_spi_execute(ret);
    }

    /*
        appendStringInfoString(&buf, "\"header\": { ");
        for (int col = 0; col < SPI_tuptable->tupdesc->natts; col++)
        {
            if (col > 0)
                appendStringInfoString(&buf, ", ");
            const char *colname = SPI_tuptable->tupdesc->attrs[col].attname.data;
            Oid typid = SPI_gettypeid(SPI_tuptable->tupdesc, col + 1);

            // Get type name
            char *typename = format_type_be(typid);

            appendStringInfo(&buf, "\"%s\": \"%s\"", colname, typename);
        }
        appendStringInfoString(&buf, " }, ");
    */

    for (uint64 rowno = 0; rowno < SPI_processed; rowno++)
    {
        if (rowno > 0)
            appendStringInfoString(&buf, ",\n");

        appendStringInfo(&buf, "\"row%llu\": { ", (unsigned long long)(rowno + 1));

        for (int col = 0; col < SPI_tuptable->tupdesc->natts; col++)
        {
            Datum val;
            const char *colname;
            Oid typid, outFuncOid;
            bool isVarLen;
            
            if (col > 0)
                appendStringInfoString(&buf, ", ");

            val = SPI_getbinval(SPI_tuptable->vals[rowno],
                                      SPI_tuptable->tupdesc, col + 1, &format_isnull);

            colname = SPI_tuptable->tupdesc->attrs[col].attname.data;

            if (format_isnull)
            {
                appendStringInfo(&buf, "\"%s\": NULL", colname);
            }
            else
            {
                typid = SPI_gettypeid(SPI_tuptable->tupdesc, col + 1);
                getTypeOutputInfo(typid, &outFuncOid, &isVarLen);

                outstr = OidOutputFunctionCall(outFuncOid, val);
                appendStringInfo(&buf, "\"%s\": \"%s\"", colname, outstr);
            }
        }
        appendStringInfoString(&buf, " }");
    }

    appendStringInfoChar(&buf, '}');
    SPI_finish();
    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}
