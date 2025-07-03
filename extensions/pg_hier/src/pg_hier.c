#include "pg_hier.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

struct table_stack {
    char *table_name;
    struct table_stack *next;
};

Datum pg_hier_parse(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pg_hier_parse);

static void parse_input(StringInfo buf, const char *input, struct table_stack **stack);
static char *trim_whitespace(char *str);

Datum
pg_hier_parse(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    elog(INFO, "pg_hier_parse called");

    text *input_text = PG_GETARG_TEXT_PP(0);
    char *input = text_to_cstring(input_text);
    StringInfoData buf;
    struct table_stack *stack = NULL;

    initStringInfo(&buf);
    appendStringInfoString(&buf, "SELECT ");
    parse_input(&buf, input, &stack); 

    appendStringInfoString(&buf, " FROM ");

    char *start = strchr(input, '{');
    char *default_table = NULL;
    if (start)
    {
        *start = '\0'; 
        default_table = trim_whitespace(input);
        if(strlen(default_table) == 0) {
            default_table = NULL;
        }
        *start = '{'; 
    }
    if (default_table)
    {
        appendStringInfoString(&buf, default_table);
    }
    else
    {
        appendStringInfoString(&buf, "some_table");
    }
    pfree(input);

    if (stack != NULL)
    {
        while (stack != NULL)
        {
            struct table_stack *top = stack;
            stack = stack->next;
            pfree(top->table_name);
            pfree(top);
        }
        ereport(ERROR, (errmsg("Unmatched opening brace in input")));
    }
    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

static void
parse_input(StringInfo buf, const char *input, struct table_stack **stack)
{
    char *input_copy = pstrdup(input);
    char *token;
    char *next_token = NULL;
    char *prev_token = NULL;
    char *saveptr;
    bool first = true;

    token = strtok_r(input_copy, ", \n\t", &saveptr);

    struct table_stack *new_entry = palloc(sizeof(struct table_stack));
    new_entry->table_name = pstrdup(token);
    new_entry->next = *stack;
    *stack = new_entry;
    token = strtok_r(NULL, ", \n\t", &saveptr);

    if (token == NULL || *token != '{')
    {
        ereport(ERROR, (errmsg("Expected opening brace '{' after table name")));
    }
    token = strtok_r(NULL, ", \n\t", &saveptr);

    while (token != NULL)
    {
        token = trim_whitespace(token);

        if (*token == '\0')
        {
            token = strtok_r(NULL, ", \n\t", &saveptr);
            continue;
        }
        if (strcmp(token, "}") == 0)
        {
            if (*stack == NULL)
            {
                ereport(ERROR, (errmsg("Unmatched closing brace in input")));
            }
            struct table_stack *top = *stack;
            *stack = (*stack)->next;
            pfree(top->table_name);
            pfree(top);
            token = strtok_r(NULL, ", \n\t", &saveptr);
            continue;
        }

        next_token = strtok_r(NULL, ", \n\t", &saveptr);

        if (next_token && *next_token == '{')
        {
            struct table_stack *new_entry = palloc(sizeof(struct table_stack));
            new_entry->table_name = pstrdup(token);
            new_entry->next = *stack;
            *stack = new_entry;
            token = strtok_r(NULL, ", \n\t", &saveptr);
            continue;
        }

        if (!first)
        {
            appendStringInfoString(buf, ", ");
        }
        first = false;
        
        if (stack && *stack && (*stack)->table_name)
        {
            appendStringInfo(buf, "%s.%s", (*stack)->table_name, token);
        }
        else
        {
            appendStringInfoString(buf, token);
        }

        prev_token = token;
        token = next_token;
        next_token = NULL;
    }

    pfree(input_copy);
}

static char *
trim_whitespace(char *str)
{
    if (str == NULL) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0)  /* All spaces? */
        return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

PG_FUNCTION_INFO_V1(pg_hier_join);

Datum
pg_hier_join(PG_FUNCTION_ARGS)
{
    text *parent_name_text = PG_GETARG_TEXT_PP(0);
    text *child_name_text = PG_GETARG_TEXT_PP(1);

    char *parent_name = text_to_cstring(parent_name_text);
    char *child_name = text_to_cstring(child_name_text);

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
        "    FROM pg_hier_table "
        "    WHERE name = $1 "
        "    UNION ALL "
        "    SELECT "
        "        h.id, h.name, h.parent_id, h.child_id, h.parent_key, h.child_key, "
        "        p.name_path || h.name, "
        "        p.key_path || pg_hier_make_key_step(h.parent_key, h.child_key), "
        "        pg_typeof(pg_hier_make_key_step(h.parent_key, h.child_key)), "
        "        pg_typeof(p.key_path) "
        "    FROM pg_hier_table h "
        "    JOIN path p ON h.parent_id = p.id "
        ") "
        "SELECT name_path, key_path "
        "FROM path "
        "WHERE name = $2;";

    Oid argtypes[2] = { TEXTOID, TEXTOID };
    Datum values[2] = { PointerGetDatum(parent_name_text), PointerGetDatum(child_name_text) };
    ret = SPI_execute_with_args(fetch_hier, 2, argtypes, values, NULL, true, 0);
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        elog(ERROR, "SPI_execute_with_args failed: %d", ret);
    }

    if (SPI_processed == 0) {
        SPI_finish();
        elog(ERROR, "No path found from %s to %s", parent_name, child_name);
    }

    HeapTuple tuple = SPI_tuptable->vals[0];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;

    bool isnull;

    Datum name_path_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
    Datum key_path_datum  = SPI_getbinval(tuple, tupdesc, 2, &isnull);

    ArrayType *name_path_arr = DatumGetArrayTypeP(name_path_datum);
    ArrayType *key_path_arr  = DatumGetArrayTypeP(key_path_datum);

    int name_path_len = ArrayGetNItems(ARR_NDIM(name_path_arr), ARR_DIMS(name_path_arr));
    int key_path_len  = ArrayGetNItems(ARR_NDIM(key_path_arr), ARR_DIMS(key_path_arr));

    if (name_path_len < 2) {
        SPI_finish();
        elog(ERROR, "No path found from %s to %s", parent_name, child_name);
    }

    Datum *name_path_elems;
    bool *name_path_nulls;
    int name_path_count;
    deconstruct_array(name_path_arr, TEXTOID, -1, false, 'i',
                      &name_path_elems, &name_path_nulls, &name_path_count);

    Datum *key_path_elems;
    bool *key_path_nulls;
    int key_path_count;
    deconstruct_array(key_path_arr, TEXTOID, -1, false, 'i',
                      &key_path_elems, &key_path_nulls, &key_path_count);

    resetStringInfo(&join_sql);
    appendStringInfo(&join_sql, "FROM %s", TextDatumGetCString(name_path_elems[0]));

    for (int i = 1; i < name_path_count; i++) {
        char *key_json = TextDatumGetCString(key_path_elems[i]);
        // Example: key_json = ["item_id:product_id","foo:bar"]

        char *key_array = key_json;
        while (*key_array && (*key_array == '[' || *key_array == ' ')) key_array++;
        char *end_array = key_array + strlen(key_array) - 1;
        while (end_array > key_array && (*end_array == ']' || *end_array == ' ')) *end_array-- = '\0';

        char *saveptr = NULL;
        char *pair = strtok_r(key_array, ",", &saveptr);

        appendStringInfo(&join_sql, " JOIN %s ON ", TextDatumGetCString(name_path_elems[i]));

        int key_idx = 0;
        while (pair) {
            while (*pair && (*pair == '"' || *pair == ' ')) pair++;
            char *end_quote = strchr(pair, '"');
            if (end_quote) *end_quote = '\0';

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
                TextDatumGetCString(name_path_elems[i-1]), left_key,
                TextDatumGetCString(name_path_elems[i]), right_key
            );

            key_idx++;
            pair = strtok_r(NULL, ",", &saveptr);
        }
    }

    SPI_finish();

    PG_RETURN_TEXT_P(cstring_to_text(join_sql.data));
}

PG_FUNCTION_INFO_V1(pg_hier_format);

Datum
pg_hier_format(PG_FUNCTION_ARGS)
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
        if (rowno > 0) appendStringInfoString(&buf, ",\n");

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
