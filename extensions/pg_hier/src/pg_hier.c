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
Datum
pg_hier(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    text *input_text = PG_GETARG_TEXT_PP(0);
    char *input = text_to_cstring(input_text);
    StringInfoData parse_buf;
    initStringInfo(&parse_buf);

    struct table_stack *stack = NULL;
    struct string_array *tables = NULL;

    appendStringInfoString(&parse_buf, "SELECT ");
    parse_input(&parse_buf, input, &stack, &tables);

    if (stack != NULL) {
        while (stack != NULL) {
            struct table_stack *top = stack;
            stack = stack->next;
            pfree(top->table_name);
            pfree(top);
        }
        ereport(ERROR, (errmsg("Unmatched opening brace in input")));
    }


    if (tables == NULL || tables->size < 2) {
        if (tables == NULL) {
            pfree(input);
            pfree(parse_buf.data);
            ereport(ERROR, (errmsg("No tables found in input string.")));
        } else {
            pfree(input);
            pfree(parse_buf.data);
            free_string_array(tables);
            ereport(ERROR, (errmsg("At least two tables are needed for hierarchical query.")));
        }
    }

    int hier_id = pg_hier_find_hier(tables);

    char *parent_table = pstrdup(tables->data[0]);
    char *child_table = pstrdup(tables->data[tables->size - 1]);

    pfree(input);
    if(tables->data)
        for (int i = 0; i < tables->size; ++i) {
            pfree(tables->data[i]);
        }
    pfree(tables->data);
    pfree(tables);

    StringInfoData join_buf;
    initStringInfo(&join_buf);

    Datum join_result = DirectFunctionCall2(pg_hier_join, CStringGetTextDatum(parent_table), CStringGetTextDatum(child_table));
    text *from_join_clause = DatumGetTextPP(join_result);
    char *from_join_clause_str = text_to_cstring(from_join_clause);
    appendStringInfoString(&join_buf, from_join_clause_str);

    pfree(from_join_clause_str);
    pfree(parent_table);
    pfree(child_table);
    appendStringInfoString(&parse_buf, " ");
    appendStringInfoString(&parse_buf, join_buf.data);
    pfree(join_buf.data);

    PG_RETURN_TEXT_P(cstring_to_text(parse_buf.data));
}

PG_FUNCTION_INFO_V1(pg_hier_parse);
/**************************************
 * CREATE FUNCTION pg_hier_parse(text) 
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier_parse'
 * LANGUAGE C STRICT;
**************************************/
Datum
pg_hier_parse(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    MemoryContext oldcontext = CurrentMemoryContext;
    text *input_text = PG_GETARG_TEXT_PP(0);
    char *input = NULL;
    StringInfoData buf;
    struct table_stack *stack = NULL;
    struct string_array *tables = NULL;
    text *result = NULL;

    elog(DEBUG1, "Starting pg_hier_parse");

    PG_TRY();
    {
        input = text_to_cstring(input_text);
        elog(DEBUG1, "Input string: %s", input);
        
        initStringInfo(&buf);
        appendStringInfoString(&buf, "SELECT ");
        
        MemoryContextSwitchTo(oldcontext);
        parse_input(&buf, input, &stack, &tables);
        
        elog(DEBUG1, "Parse complete, result: %s", buf.data);
        
        result = cstring_to_text(buf.data);
        
        if (buf.data != NULL)
            pfree(buf.data);
        if (input != NULL)
            pfree(input);
        if (stack != NULL)
            free_table_stack(stack);
        if (tables != NULL)
            free_string_array(tables);
            
        PG_RETURN_TEXT_P(result);
    }
    PG_CATCH();
    {
        elog(WARNING, "Caught exception in pg_hier_parse");
        
        if (input != NULL)
        {
            elog(WARNING, "Input not null");
            pfree(input);
        }
        if (buf.data != NULL)
        {
            elog(WARNING, "buf.data not null");
            pfree(buf.data);
        }
        if (stack != NULL)
        {
            elog(WARNING, "stack not null");
            free_table_stack(stack);
        }
        if (tables != NULL)
        {
            elog(WARNING, "tables not null");
            free_string_array(tables);
            elog(WARNING, "Reached end of function");
        }
            
        PG_RE_THROW();
    }
    PG_END_TRY();
}

PG_FUNCTION_INFO_V1(pg_hier_join);
/**************************************
 * CREATE FUNCTION pg_hier_join(TEXT, TEXT)
 * RETURNS text
 * AS 'MODULE_PATHNAME', 'pg_hier_join'
 * LANGUAGE C STRICT;
**************************************/
Datum
pg_hier_join(PG_FUNCTION_ARGS)
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

    deconstruct_array(name_path_arr, TEXTOID, -1, false, 'i',
                      &name_path_elems, &name_path_nulls, &name_path_count);

    
    deconstruct_array(key_path_arr, TEXTOID, -1, false, 'i',
                      &key_path_elems, &key_path_nulls, &key_path_count);

    resetStringInfo(&join_sql);
    appendStringInfo(&join_sql, "FROM %s", TextDatumGetCString(name_path_elems[0]));

    for (int i = 1; i < name_path_count; i++) {
        parent_table = pstrdup(TextDatumGetCString(name_path_elems[i-1]));
        join_table = pstrdup(TextDatumGetCString(name_path_elems[i]));
        key_json = TextDatumGetCString(key_path_elems[i]);
        // Skip leading brackets and spaces
        key_array = key_json;
        while (*key_array && (*key_array == '[' || *key_array == ' ')) key_array++;

        // Remove trailing brackets and spaces
        end_array = key_array + strlen(key_array) - 1;
        while (end_array > key_array && (*end_array == ']' || *end_array == ' ')) {
            *end_array = '\0';
            end_array--;
        }

        char *pair = strtok_r(key_array, ",", &saveptr);
        appendStringInfo(&join_sql, " JOIN %s ON ", join_table);
        key_idx = 0;
        while (pair) {
            // Skip leading quotes and spaces
            while (*pair && (*pair == '"' || *pair == ' ')) pair++;
            
            // Find ending quote and terminate string there
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

            elog(INFO, "Current table check 2: %s", 
                TextDatumGetCString(name_path_elems[i]));
                
            appendStringInfo(&join_sql, "%s.%s = %s.%s",
                parent_table, left_key,
                join_table, right_key
            );
            
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

PG_FUNCTION_INFO_V1(pg_hier_col_to_json_sfunc);
/**************************************
 * Transition function for column array aggregation
 * Takes column_name:text, column_value pairs as arguments
 **************************************/
Datum
pg_hier_col_to_json_sfunc(PG_FUNCTION_ARGS)
{
    MemoryContext agg_context;
    MemoryContext old_context;
    ColumnArrayState *state;
    
    /* Check if called as an aggregate */
    if (!AggCheckCallContext(fcinfo, &agg_context))
        elog(ERROR, "pg_hier_col_to_json_sfunc called in non-aggregate context");
        
    /* Get or initialize the state */
    if (PG_ARGISNULL(0)) {
        old_context = MemoryContextSwitchTo(agg_context);
        state = (ColumnArrayState *) palloc0(sizeof(ColumnArrayState));
        state->allocated = 8;  /* Initial capacity */
        state->num_columns = 0;
        state->column_names = (text **) palloc(sizeof(text *) * state->allocated);
        state->column_values = (List **) palloc(sizeof(List *) * state->allocated);
        state->column_nulls = (List **) palloc(sizeof(List *) * state->allocated);
        state->column_types = (Oid *) palloc(sizeof(Oid) * state->allocated);
        MemoryContextSwitchTo(old_context);
    } else {
        state = (ColumnArrayState *) PG_GETARG_POINTER(0);
    }
    
    /* Check if we have a key-value pair */
    if (PG_NARGS() < 3 || PG_NARGS() % 2 != 1)
        elog(ERROR, "Expected key-value pairs, got odd number of arguments");
        
    /* Process each key-value pair */
    for (int i = 1; i < PG_NARGS(); i += 2) {
        /* Process key (should be text) */
        if (PG_ARGISNULL(i))
            elog(ERROR, "Column name cannot be NULL");
        
        text *column_name = PG_GETARG_TEXT_PP(i);
        
        /* Check if this column already exists in our state */
        int col_idx = find_column_index(state, column_name);
        
        /* If this is a new column, add it */
        if (col_idx < 0) {
            /* Resize arrays if necessary */
            if (state->num_columns >= state->allocated) {
                old_context = MemoryContextSwitchTo(agg_context);
                state->allocated *= 2;
                state->column_names = (text **) repalloc(state->column_names, sizeof(text *) * state->allocated);
                state->column_values = (List **) repalloc(state->column_values, sizeof(List *) * state->allocated);
                state->column_nulls = (List **) repalloc(state->column_nulls, sizeof(List *) * state->allocated);
                state->column_types = (Oid *) repalloc(state->column_types, sizeof(Oid) * state->allocated);
                MemoryContextSwitchTo(old_context);
            }
            
            /* Add the new column */
            col_idx = state->num_columns++;
            
            old_context = MemoryContextSwitchTo(agg_context);
            
            /* Make a copy of the text in the agg context */
            text *column_name_copy = (text *) palloc(VARSIZE_ANY(column_name));
            memcpy(column_name_copy, column_name, VARSIZE_ANY(column_name));
            
            state->column_names[col_idx] = column_name_copy;
            state->column_values[col_idx] = NIL;
            state->column_nulls[col_idx] = NIL;
            state->column_types[col_idx] = InvalidOid;
            MemoryContextSwitchTo(old_context);
        }
        
        /* Process value */
        Datum value;
        bool is_null;
        Oid value_type;
        
        if (PG_ARGISNULL(i+1)) {
            is_null = true;
            value = (Datum) 0;
        } else {
            is_null = false;
            value = PG_GETARG_DATUM(i+1);
            value_type = get_fn_expr_argtype(fcinfo->flinfo, i+1);
            
            /* If this is the first value for this column, store its type */
            if (state->column_types[col_idx] == InvalidOid)
                state->column_types[col_idx] = value_type;
            
            /* If types don't match, raise an error */
            else if (state->column_types[col_idx] != value_type)
                elog(ERROR, "Column value types must be consistent within a column");
        }
        
        /* Add the value to the column's value list */
        old_context = MemoryContextSwitchTo(agg_context);
        
        /* For non-null values */
        if (!is_null) {
            if (value_type != InvalidOid) {
                int16 typlen;
                bool typbyval;
                
                get_typlenbyval(value_type, &typlen, &typbyval);
                if (typbyval) {
                    /* For by-value types, just add the datum */
                    state->column_values[col_idx] = lappend(state->column_values[col_idx], 
                                                           (void *) value);
                } else {
                    /* For by-reference types, copy the value */
                    state->column_values[col_idx] = lappend(state->column_values[col_idx], 
                                                           DatumGetPointer(datumCopy(value, false, typlen)));
                }
            }
        }
        
        /* Add null flag to the nulls list */
        state->column_nulls[col_idx] = lappend_int(state->column_nulls[col_idx], is_null ? 1 : 0);
        
        MemoryContextSwitchTo(old_context);
    }
    
    PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(pg_hier_col_to_json_ffunc);
/**************************************
 * Final function for column array aggregation
 * Converts the state to a JSONB object with arrays
 **************************************/
Datum
pg_hier_col_to_json_ffunc(PG_FUNCTION_ARGS)
{
    ColumnArrayState *state;
    
    /* Final functions can be called with NULL state */
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();
        
    state = (ColumnArrayState *) PG_GETARG_POINTER(0);
    
    if (state->num_columns == 0)
        PG_RETURN_NULL();
        
    /* Create a new jsonb object */
    JsonbParseState *parse_state = NULL;
    JsonbValue *result;
    
    pushJsonbValue(&parse_state, WJB_BEGIN_OBJECT, NULL);
    
    /* Process each column */
    for (int i = 0; i < state->num_columns; i++) {
        /* Extract column name as text */
        text *key_text = state->column_names[i];
        char *key_cstr = text_to_cstring(key_text);
        
        JsonbValue key_value;
        key_value.type = jbvString;
        key_value.val.string.val = pstrdup(key_cstr);
        key_value.val.string.len = strlen(key_cstr);
        
        pushJsonbValue(&parse_state, WJB_KEY, &key_value);
        
        /* Begin array for this column */
        pushJsonbValue(&parse_state, WJB_BEGIN_ARRAY, NULL);
        
        /* Get the column values and null flags */
        List *values = state->column_values[i];
        List *nulls = state->column_nulls[i];
        Oid type_id = state->column_types[i];
        
        /* Add each value to the array */
        ListCell *val_cell, *null_cell;
        forboth(val_cell, values, null_cell, nulls)
        {
            int is_null = lfirst_int(null_cell);
            
            if (is_null) {
                /* Add NULL value */
                JsonbValue null_value;
                null_value.type = jbvNull;
                pushJsonbValue(&parse_state, WJB_ELEM, &null_value);
            } else {
                /* Add non-NULL value */
                Datum val_datum;
                if (type_id != InvalidOid) {
                    int16 typlen;
                    bool typbyval;
                    
                    get_typlenbyval(type_id, &typlen, &typbyval);
                    if (typbyval)
                        val_datum = (Datum) lfirst(val_cell);
                    else
                        val_datum = PointerGetDatum(lfirst(val_cell));
                    
                    JsonbValue jbv;
                    datum_to_jsonb(val_datum, type_id, &jbv);
                    pushJsonbValue(&parse_state, WJB_ELEM, &jbv);
                }
            }
        }
        
        /* End array for this column */
        pushJsonbValue(&parse_state, WJB_END_ARRAY, NULL);
        
        pfree(key_cstr); /* Free the C string */
    }
    
    result = pushJsonbValue(&parse_state, WJB_END_OBJECT, NULL);
    
    PG_RETURN_JSONB_P(JsonbValueToJsonb(result));
}
