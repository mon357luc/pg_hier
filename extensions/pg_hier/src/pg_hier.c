#include "pg_hier.h"

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
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    text *input_text = PG_GETARG_TEXT_PP(0);
    char *input = text_to_cstring(input_text);
    StringInfoData parse_buf;
    initStringInfo(&parse_buf);

    string_array *tables = NULL;

    appendStringInfoString(&parse_buf, "SELECT ");
    parse_input(&parse_buf, input, &tables);

    if (tables == NULL || tables->size < 2)
    {
        if (tables == NULL)
        {
            pfree(input);
            pfree(parse_buf.data);
            ereport(ERROR, (errmsg("No tables found in input string.")));
        }
        else
        {
            pfree(input);
            pfree(parse_buf.data);
            free_string_array(tables);
            ereport(ERROR, (errmsg("At least two tables are needed for hierarchical query.")));
        }
    }

    free_string_array(tables);

    // int hier_id = pg_hier_find_hier(tables);

    // char *parent_table = pstrdup(tables->data[0]);
    // char *child_table = pstrdup(tables->data[tables->size - 1]);

    // pfree(input);
    // if (tables->data)
    //     for (int i = 0; i < tables->size; ++i)
    //     {
    //         pfree(tables->data[i]);
    //     }
    // pfree(tables->data);
    // pfree(tables);

    // StringInfoData join_buf;
    // initStringInfo(&join_buf);

    // Datum join_result = DirectFunctionCall2(pg_hier_join,
    //                                         CStringGetTextDatum(parent_table),
    //                                         CStringGetTextDatum(child_table));
    
    // text *from_join_clause = DatumGetTextPP(join_result);
    // char *from_join_clause_str = text_to_cstring(from_join_clause);
    // appendStringInfoString(&join_buf, from_join_clause_str);

    // pfree(from_join_clause_str);
    // pfree(parent_table);
    // pfree(child_table);
    // appendStringInfoString(&parse_buf, " ");
    // appendStringInfoString(&parse_buf, join_buf.data);
    // pfree(join_buf.data);

    PG_RETURN_TEXT_P(cstring_to_text(parse_buf.data));
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
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    text *input_text = PG_GETARG_TEXT_PP(0);
    char *input = text_to_cstring(input_text);
    StringInfoData parse_buf;
    initStringInfo(&parse_buf);

    string_array *tables = NULL;

    appendStringInfoString(&parse_buf, "SELECT ");
    parse_input(&parse_buf, input, &tables);

    if (tables == NULL || tables->size < 2)
    {
        if (tables == NULL)
        {
            pfree(input);
            pfree(parse_buf.data);
            ereport(ERROR, (errmsg("No tables found in input string.")));
        }
        else
        {
            pfree(input);
            pfree(parse_buf.data);
            free_string_array(tables);
            ereport(ERROR, (errmsg("At least two tables are needed for hierarchical query.")));
        }
    }
    pfree(input);
    free_string_array(tables);

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

    text *parent_name_text = PG_GETARG_TEXT_PP(0);
    text *child_name_text = PG_GETARG_TEXT_PP(1);

    char *parent_name = text_to_cstring(parent_name_text);
    char *child_name = text_to_cstring(child_name_text);

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
    initStringInfo(&join_sql);

    int ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed: %d", ret);

    const char *fetch_hier =
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

    Oid argtypes[2] = {TEXTOID, TEXTOID};
    Datum values[2] = {PointerGetDatum(parent_name_text), PointerGetDatum(child_name_text)};
    ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        elog(ERROR, "SPI_execute_with_args failed: %d", ret);
    }

    if (SPI_processed == 0)
    {
        SPI_finish();
        elog(ERROR, "No path found from %s to %s", parent_name, child_name);
    }

    HeapTuple tuple = SPI_tuptable->vals[0];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;

    Datum name_path_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
    Datum key_path_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);

    ArrayType *name_path_arr = DatumGetArrayTypeP(name_path_datum);
    ArrayType *key_path_arr = DatumGetArrayTypeP(key_path_datum);

    int name_path_len = ArrayGetNItems(ARR_NDIM(name_path_arr), ARR_DIMS(name_path_arr));
    int key_path_len = ArrayGetNItems(ARR_NDIM(key_path_arr), ARR_DIMS(key_path_arr));

    if (name_path_len < 2)
    {
        SPI_finish();
        elog(ERROR, "No path found from %s to %s", parent_name, child_name);
    }

    deconstruct_array(name_path_arr, TEXTOID, -1, false, 'i',
                      &name_path_elems, &name_path_nulls, &name_path_count);

    deconstruct_array(key_path_arr, TEXTOID, -1, false, 'i',
                      &key_path_elems, &key_path_nulls, &key_path_count);

    resetStringInfo(&join_sql);
    appendStringInfo(&join_sql, "FROM %s", TextDatumGetCString(name_path_elems[0]));

    for (int i = 1; i < name_path_count; i++)
    {
        parent_table = pstrdup(TextDatumGetCString(name_path_elems[i - 1]));
        join_table = pstrdup(TextDatumGetCString(name_path_elems[i]));
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

        char *pair = strtok_r(key_array, ",", &saveptr);
        appendStringInfo(&join_sql, " JOIN %s ON ", join_table);
        key_idx = 0;
        while (pair)
        {
            while (*pair && (*pair == '"' || *pair == ' '))
                pair++;

            char *end_quote = strchr(pair, '"');
            if (end_quote)
                *end_quote = '\0';

            char *colon = strchr(pair, ':');
            if (!colon)
            {
                SPI_finish();
                elog(ERROR, "Invalid key format: %s", pair);
            }
            *colon = '\0';
            char *left_key = pair;
            char *right_key = colon + 1;

            if (key_idx > 0)
                appendStringInfoString(&join_sql, " AND ");

            elog(INFO, "Current table check 2: %s",
                 TextDatumGetCString(name_path_elems[i]));

            appendStringInfo(&join_sql, "%s.%s = %s.%s",
                             parent_table, left_key,
                             join_table, right_key);

            key_idx++;
            pair = strtok_r(NULL, ",", &saveptr);
        }
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
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    text *sql = PG_GETARG_TEXT_PP(0);
    char *query = text_to_cstring(sql);

    StringInfoData buf;
    initStringInfo(&buf);
    appendStringInfoChar(&buf, '{');

    int ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed: %d", ret);

    ret = SPI_execute(query, true, 0);
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        elog(ERROR, "SPI_execute failed: %d", ret);
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
            if (col > 0)
                appendStringInfoString(&buf, ", ");

            bool isnull;
            Datum val = SPI_getbinval(SPI_tuptable->vals[rowno],
                                      SPI_tuptable->tupdesc, col + 1, &isnull);

            const char *colname = SPI_tuptable->tupdesc->attrs[col].attname.data;

            if (isnull)
            {
                appendStringInfo(&buf, "\"%s\": NULL", colname);
            }
            else
            {
                Oid typid = SPI_gettypeid(SPI_tuptable->tupdesc, col + 1);
                Oid outFuncOid;
                bool isVarlen;
                getTypeOutputInfo(typid, &outFuncOid, &isVarlen);

                char *outstr = OidOutputFunctionCall(outFuncOid, val);
                appendStringInfo(&buf, "\"%s\": \"%s\"", colname, outstr);
            }
        }
        appendStringInfoString(&buf, " }");
    }

    appendStringInfoChar(&buf, '}');
    SPI_finish();
    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

// Helper for parsing input data in pg_hier_create_hier. Returns new value for
// i.
int get_next_token(char *input_string, int i, char **token_return_param) {
    *token_return_param = input_string + i;
    while (input_string[i] != '\0' && input_string[i] != ' ' &&
           input_string[i] != '\t' && input_string[i] != '\r' &&
           input_string[i] != '\n') {
      i++;
    }

    // The current character is now whitespace. We can safely overwrite it,
    // splitting the input string.
    input_string[i] = '\0';
    i++;

    // elog(INFO, "Found token: '%s'", *token_return_param);

    return i;
}

PG_FUNCTION_INFO_V1(pg_hier_create_hier_nosuck);

/**************************************
 * CREATE FUNCTION pg_hier_create_hier_nosuck(TEXT)
 * AS 'MODULE_PATHNAME', 'pg_hier_create_hier_nosuck'
 * RETURNS text;
 * LANGUAGE C STRICT;
 **************************************/
Datum pg_hier_create_hier_nosuck(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    text *input_text = PG_GETARG_TEXT_PP(0);
    char *input_string = text_to_cstring(input_text);

    int i;
    char *token = NULL;
    char *parent_name = NULL;
    char *child_name = NULL;
    // For 'on' statements, only the parent key should be used. For 'using'
    // statements, both parent and child keys are needed.
    char *parent_key = NULL;
    char *child_key = NULL;

    int hier_id = 1; // TODO implement this counter

    i = 0;
    while (input_string[i] != '\0') {
        if (input_string[i] == '\r' || input_string[i] == '\n' || input_string[i] == '\t') {
            i++;
            continue;
        }

        // elog(INFO, "Parsing line...", input_string);

        i = get_next_token(input_string, i, &parent_name);
        i = get_next_token(input_string, i, &token);
        // NOTE no need to use the safer strncmp because get_next_token ensures
        // that the token is proper null-terminated
        if (strcmp("has", token) != 0) {
            elog(ERROR, "Expected 'has' after '%s' (position %d)", parent_name, i);
        }
        i = get_next_token(input_string, i, &token);
        if (strcmp("child", token) != 0) {
            elog(ERROR, "Expected 'child' after '%s has' (position %d)", parent_name, i);
        }
        i = get_next_token(input_string, i, &child_name);

        i = get_next_token(input_string, i, &token);
        if (strcmp("using", token) == 0) {
            // expect 'key'
            i = get_next_token(input_string, i, &parent_key);
            elog(INFO, "Parsed hierarchy relationship: '%s' has child '%s' using '%s'", parent_name, child_name, parent_key);
            // TODO now we'll do some kind of SPI call like this:
            // INSERT INTO pg_hier_detail (parent, child, parent_key, child_key) VALUES (parent_name, child_name, parent_key, parent_key);
            // (once we know what the schema of that table is)
        } else if (strcmp("on", token) == 0) {

            elog(ERROR, "'on' not yet supported, use 'using' instead");

            // expect 'name1.key = name2.key'

            // token = input_string + i;
            // while (input_string[i] != '.') {
            //     i++;
            // }
            // input_string[i] = '\0';
            // i++;
            // elog(INFO, "Found LHS of 'on' token: '%s'", token);
            // if (strcmp(parent_name, token) == 0) {
            // } else if (strcmp(child_name, token) == 0) {
            // } else {
            //     // elog(ERROR, "Unrecognized name '%s', expected '%s' or '%s' (position %d)");
            // }
            // i = get_next_token(input_string, i, &token);
            // elog(INFO, "Found RHS of 'on' token: '%s'", token);
            //
            // i = get_next_token(input_string, i, &token);
            // if (strcmp("=", token) == 0 {
            //     // elog(ERROR, "Expected '=' after 'on %s.%s' (position %d)", , ,i);
            // }
            //
            // token = input_string + i;
            // while (input_string[i] != '.') {
            //     i++;
            // }
            // input_string[i] = '\0';
            // i++;
            // elog(INFO, "Found LHS of 'on' token: '%s'", token);
            // i = get_next_token(input_string, i, &token);
            // elog(INFO, "Found RHS of 'on' token: '%s'", token);

            // elog(INFO, "Parsed hierarchy relationship: '%s' has child '%s' on '%s.%s = %s.%s'", parent_name, child_name, parent_key);

        } else {
            elog(ERROR, "Expected 'on' or 'using' after child '%s' (position %d)", child_name, i);
        }

    }

    pfree(input_string);

    PG_RETURN_TEXT_P(cstring_to_text("Done :)")); // this return value is pointless
}
