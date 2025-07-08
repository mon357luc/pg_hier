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
 * CREATE FUNCTION pg_hier(text, text DEFAULT 'auto')
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
    char *direction = NULL;
    if (PG_NARGS() > 1 && !PG_ARGISNULL(1)) {
        text *dir_text = PG_GETARG_TEXT_PP(1);
        direction = text_to_cstring(dir_text);
    } else {
        direction = "auto";
    }

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

    // Build join clause using new bidirectional join logic
    char *parent_table = pstrdup(tables->data[0]);
    char *child_table = pstrdup(tables->data[tables->size - 1]);

    StringInfoData join_buf;
    initStringInfo(&join_buf);

    // Call pg_hier_join with direction
    Datum join_result = DirectFunctionCall3(pg_hier_join,
                                            CStringGetTextDatum(parent_table),
                                            CStringGetTextDatum(child_table),
                                            CStringGetTextDatum(cstring_to_text(direction)));
    text *from_join_clause = DatumGetTextPP(join_result);
    char *from_join_clause_str = text_to_cstring(from_join_clause);
    appendStringInfoString(&join_buf, from_join_clause_str);

    pfree(from_join_clause_str);
    pfree(parent_table);
    pfree(child_table);
    appendStringInfoString(&parse_buf, " ");
    appendStringInfoString(&parse_buf, join_buf.data);
    pfree(join_buf.data);

    free_string_array(tables);
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
 * CREATE FUNCTION pg_hier_join(TEXT, TEXT, TEXT)
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier_join'
 * LANGUAGE C STRICT;
 **************************************/
Datum pg_hier_join(PG_FUNCTION_ARGS)
{
    bool isnull;

    text *parent_name_text = PG_GETARG_TEXT_PP(0);
    text *child_name_text = PG_GETARG_TEXT_PP(1);
    text *direction_text = PG_NARGS() > 2 && !PG_ARGISNULL(2) ? PG_GETARG_TEXT_PP(2) : NULL;
    char *parent_name = text_to_cstring(parent_name_text);
    char *child_name = text_to_cstring(child_name_text);
    char *direction = direction_text ? text_to_cstring(direction_text) : "auto";

    Datum *name_path_elems = NULL;
    bool *name_path_nulls = NULL;
    int name_path_count = 0;
    Datum *key_path_elems = NULL;
    bool *key_path_nulls = NULL;
    int key_path_count = 0;

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

    // Bidirectional recursive CTE
    const char *fetch_hier_down =
        "WITH RECURSIVE path AS ( "
        "    SELECT id, name, parent_id, child_id, parent_key, child_key, ARRAY[name] AS name_path, COALESCE(pg_hier_make_key_step(parent_key, child_key), '{[]}') AS key_path "
        "    FROM pg_hier_detail WHERE name = $1 "
        "    UNION ALL "
        "    SELECT h.id, h.name, h.parent_id, h.child_id, h.parent_key, h.child_key, p.name_path || h.name, p.key_path || pg_hier_make_key_step(h.parent_key, h.child_key) "
        "    FROM pg_hier_detail h JOIN path p ON h.parent_id = p.id "
        ") SELECT name_path, key_path FROM path WHERE name = $2;";

    const char *fetch_hier_up =
        "WITH RECURSIVE path AS ( "
        "    SELECT id, name, parent_id, child_id, parent_key, child_key, ARRAY[name] AS name_path, COALESCE(pg_hier_make_key_step(parent_key, child_key), '{[]}') AS key_path "
        "    FROM pg_hier_detail WHERE name = $1 "
        "    UNION ALL "
        "    SELECT h.id, h.name, h.parent_id, h.child_id, h.parent_key, h.child_key, p.name_path || h.name, p.key_path || pg_hier_make_key_step(h.parent_key, h.child_key) "
        "    FROM pg_hier_detail h JOIN path p ON h.child_id = p.id "
        ") SELECT name_path, key_path FROM path WHERE name = $2;";

    const char *fetch_hier = NULL;
    Oid argtypes[2] = {TEXTOID, TEXTOID};
    Datum values[2] = {PointerGetDatum(parent_name_text), PointerGetDatum(child_name_text)};
    bool found = false;

    if (strcmp(direction, "down") == 0) {
        fetch_hier = fetch_hier_down;
        ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
        found = (ret == SPI_OK_SELECT && SPI_processed > 0);
    } else if (strcmp(direction, "up") == 0) {
        fetch_hier = fetch_hier_up;
        ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
        found = (ret == SPI_OK_SELECT && SPI_processed > 0);
    } else {
        // Try down first, then up
        fetch_hier = fetch_hier_down;
        ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
        if (ret == SPI_OK_SELECT && SPI_processed > 0) {
            found = true;
        } else {
            fetch_hier = fetch_hier_up;
            ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
            found = (ret == SPI_OK_SELECT && SPI_processed > 0);
        }
    }

    if (!found) {
        SPI_finish();
        elog(ERROR, "No path found from %s to %s (direction: %s)", parent_name, child_name, direction);
    }

    HeapTuple tuple = SPI_tuptable->vals[0];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;

    Datum name_path_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
    Datum key_path_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);

    ArrayType *name_path_arr = DatumGetArrayTypeP(name_path_datum);
    ArrayType *key_path_arr = DatumGetArrayTypeP(key_path_datum);

    int name_path_len = ArrayGetNItems(ARR_NDIM(name_path_arr), ARR_DIMS(name_path_arr));
    int key_path_len = ArrayGetNItems(ARR_NDIM(key_path_arr), ARR_DIMS(key_path_arr));

    if (name_path_len < 2) {
        SPI_finish();
        elog(ERROR, "No path found from %s to %s (direction: %s)", parent_name, child_name, direction);
    }

    deconstruct_array(name_path_arr, TEXTOID, -1, false, 'i',
                      &name_path_elems, &name_path_nulls, &name_path_count);
    deconstruct_array(key_path_arr, TEXTOID, -1, false, 'i',
                      &key_path_elems, &key_path_nulls, &key_path_count);

    resetStringInfo(&join_sql);
    appendStringInfo(&join_sql, "FROM %s", TextDatumGetCString(name_path_elems[0]));

    for (int i = 1; i < name_path_count; i++) {
        parent_table = pstrdup(TextDatumGetCString(name_path_elems[i - 1]));
        join_table = pstrdup(TextDatumGetCString(name_path_elems[i]));
        key_json = TextDatumGetCString(key_path_elems[i]);
        key_array = key_json;
        while (*key_array && (*key_array == '[' || *key_array == ' '))
            key_array++;
        end_array = key_array + strlen(key_array) - 1;
        while (end_array > key_array && (*end_array == ']' || *end_array == ' ')) {
            *end_array = '\0';
            end_array--;
        }
        char *pair = strtok_r(key_array, ",", &saveptr);
        appendStringInfo(&join_sql, " JOIN %s ON ", join_table);
        key_idx = 0;
        while (pair) {
            while (*pair && (*pair == '"' || *pair == ' '))
                pair++;
            char *end_quote = strchr(pair, '"');
            if (end_quote)
                *end_quote = '\0';
            char *colon = strchr(pair, ':');
            if (!colon) {
                SPI_finish();
                elog(ERROR, "Invalid key format: %s", pair);
            }
            *colon = '\0';
            char *left_key = pair;
            char *right_key = colon + 1;
            if (key_idx > 0)
                appendStringInfoString(&join_sql, " AND ");
            appendStringInfo(&join_sql, "%s.%s = %s.%s",
                             parent_table, left_key,
                             join_table, right_key);
            key_idx++;
            pair = strtok_r(NULL, ",", &saveptr);
        }
        pfree(parent_table);
        pfree(join_table);
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
