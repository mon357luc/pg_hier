#include "pg_hier_helper.h"

void 
parse_input(StringInfo buf, const char *input, string_array **tables)
{
    table_stack *stack = NULL;
    hier_header *hh = NULL;
    char *input_copy = NULL;
    char *token = NULL;
    char *next_token = NULL;
    char *saveptr = NULL;
    bool first_column = true;
    StringInfoData where_clause;
    initStringInfo(&where_clause);
    
    PG_TRY();
    {
        *tables = create_string_array();
        hh = CREATE_HIER_HEADER();
        input_copy = pstrdup(input);
        appendStringInfoString(buf, "jsonb_agg(jsonb_build_object(");
        token = GET_TOKEN(input_copy, &saveptr);
        next_token = GET_TOKEN(&saveptr);
        if (*token == '\0')
            elog(ERROR, "Expected table name");
        add_string_to_array(*tables, token);
        if (!first_column)
            appendStringInfoString(buf, ", ");
        stack = create_table_stack_entry(token, stack);
        token = GET_TOKEN(&saveptr);
        next_token = GET_TOKEN(&saveptr);
        while (token)
        {
            if (*token == '\0')
                break;
            if (next_token && !strcmp(next_token, "{"))
            {
                int hier_id;
                add_string_to_array(*tables, token);
                stack = create_table_stack_entry(token, stack);
                if (!first_column)
                    appendStringInfoString(buf, ", ");
                appendStringInfo(buf, 
                    "'%s', (SELECT jsonb_agg(json_build_object(", token);
                first_column = true;
                token = GET_TOKEN(&saveptr);
                next_token = GET_TOKEN(&saveptr);
            }
            else
            {
                while (token)
                {
                    if (*token == '\0')
                        break;
                    if (next_token && !strcmp(next_token, "{"))
                        break;
                    if (token && strcmp(token, "}") == 0)
                    {
                        if (stack == NULL)
                            ereport(ERROR, 
                                (errmsg("Unmatched closing brace in input")));
                        StringInfoData rel_buf;
                        initStringInfo(&rel_buf);
                        char *child_table = pop_table_stack(&stack);
                        char *parent_table = NULL;
                        
                        token = next_token;
                        next_token = GET_TOKEN(&saveptr);
                        
                        StringInfoData where_condition;
                        initStringInfo(&where_condition);
                        
                        if (token && !strcmp(token, "WHERE")) {
                            while (next_token && strcmp(next_token, "}") != 0 && strcmp(next_token, ";")) {
                                appendStringInfo(&where_condition, "%s ", next_token);
                                token = next_token;
                                next_token = GET_TOKEN(&saveptr);
                                if (!next_token) break;
                            }
                        }
                        
                        if(stack)
                        {
                            parent_table = peek_table_stack(&stack);
                            pg_hier_get_hier(*tables, hh);
                            pg_hier_from_clause(&rel_buf, hh, parent_table, child_table);
                            appendStringInfo(buf, ")) FROM %s", rel_buf.data);
                            
                            if (where_condition.len > 0)
                                appendStringInfo(buf, " AND %s", where_condition.data);
                            
                            appendStringInfoString(buf, " )");
                        } else {
                            appendStringInfo(buf, ")) FROM %s", child_table);
                            
                            if (where_condition.len > 0)
                                appendStringInfo(buf, " WHERE %s", where_condition.data);
                            pfree(where_condition.data);
                            if (child_table)
                                pfree(child_table);
                            token = NULL;
                            next_token = NULL;
                        }
                            
                        if (!token || !next_token) break;
                        
                        if (!strcmp(token, "WHERE")) {
                            token = next_token;
                            next_token = GET_TOKEN(&saveptr);
                        } else {
                            token = token;
                            next_token = next_token;
                        }
                        continue;
                    }
                    if (first_column)
                        first_column = false;
                    else
                        appendStringInfoString(buf, ", ");
                    appendStringInfo(buf, "'%s', %s.%s",
                                     token, stack->table_name, token);
                    token = next_token;
                    next_token = GET_TOKEN(&saveptr);
                }
            }
        }
    }
    PG_FINALLY();
    {
        if (stack)
            free_table_stack(&stack);
        if (input_copy != NULL)
            pfree(input_copy);
    }
    PG_END_TRY();
}

void
pg_hier_get_hier(string_array *tables, hier_header *hh)
{
    // Attempts to check 'cache,' if hh is null or
    // newest table isn't in hh, get new hh
    if (!hh || hh->deepest_nest < 0)
        pg_hier_find_hier(tables, hh);
    else
        for (int i = hh->deepest_nest; i < tables->size; i++)
            if(!strstr(hh->hier, tables->data[i]))
                pg_hier_find_hier(tables, hh);
    hh->deepest_nest = tables->size - 1;
}

void
pg_hier_find_hier(string_array *tables, hier_header *hh)
{
    string_array *ordered_tables = NULL;
    char *hierarchy_string = NULL;
    StringInfoData query;
    uint64 hier_id = 0;
    int ret;

    if (tables == NULL || tables->size < 2)
        ereport(ERROR, 
            (errmsg("Name path elements must contain at least two elements")));

    initStringInfo(&query);
    appendStringInfo(&query, "SELECT id, table_path FROM pg_hier_header WHERE ");
    for (int i = 0; i < tables->size; i++)
        appendStringInfo(&query, 
            (
                (i > 0) ? 
                " AND table_path LIKE '%%%s%%'" : 
                "table_path LIKE '%%%s%%'"
            ), tables->data[i]);

    PG_TRY();
    {
        if ((ret = SPI_connect()) != SPI_OK_CONNECT)
            elog(ERROR, "SPI_connect failed: %d", ret);
        
        if ((ret = SPI_execute(query.data, true, 0)) != SPI_OK_SELECT)
            elog(ERROR, "SPI_execute failed: %d", ret);
        
        if (SPI_processed > 0)
        {
            HeapTuple tuple = SPI_tuptable->vals[0];
            bool isnull;
            Datum id_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull);
            Datum path_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull);

            hier_id = DatumGetUInt64(id_datum);
            hierarchy_string = pstrdup(TextDatumGetCString(path_datum));

            if (hh)
                update_hier_header(hh, hierarchy_string, hier_id);
            else
                CREATE_HIER_HEADER(pstrdup(hierarchy_string), hier_id);

            pfree(hierarchy_string);
        }
    }
    PG_FINALLY();
    {
        SPI_finish();
        pfree(query.data);
    }
    PG_END_TRY();
}

void 
pg_hier_from_clause(StringInfo buf, hier_header *hh, char *parent, char *child)
{
    int ret;
    
    // Input validation
    if (!hh || !parent || !child) {
        elog(WARNING, "Missing required parameters for hierarchy lookup");
        if (child) appendStringInfoString(buf, child);
        return;
    }
    
    PG_TRY();
    {
        if ((ret = SPI_connect()) != SPI_OK_CONNECT)
                elog(ERROR, "SPI_connect failed: %d", ret);
                
        text *parent_text = parent ? cstring_to_text(parent) : NULL;
        text *child_text = child ? cstring_to_text(child) : NULL;
        
        Oid argtypes[3] = {INT4OID, TEXTOID, TEXTOID};
        Datum values[3] = {
            Int32GetDatum(hh->hier_id),
            PointerGetDatum(parent_text),
            PointerGetDatum(child_text)
        };
        
        ret = SPI_execute_with_args(
            PG_HIER_SQL_GET_HIER_BY_ID, 
            3, argtypes, values, NULL, true, 0);
            
        if (parent_text) pfree(parent_text);
        if (child_text) pfree(child_text);
        
        if (ret != SPI_OK_SELECT)
            elog(ERROR, "SPI_execute failed: %d", ret);
        
        if (SPI_processed > 0) {
            bool first_join = true;
            HeapTuple last_tuple = NULL;
            int last_idx = SPI_processed - 1;
            
            appendStringInfoString(buf, child);
            
            for (int i = 0; i < last_idx; i++) {
                HeapTuple tuple = SPI_tuptable->vals[i];
                bool isnull_parent, isnull_child, isnull_parent_key, isnull_child_key;
                
                Datum parent_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull_parent);
                Datum parent_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull_parent_key);
                Datum child_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 3, &isnull_child);
                Datum child_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 4, &isnull_child_key);
                
                if (isnull_parent || isnull_child || isnull_parent_key || isnull_child_key) {
                    elog(WARNING, "Row %d has NULL values in critical fields", i);
                    continue;
                }
                
                char *parent_name = TextDatumGetCString(parent_name_datum);
                char *child_name = TextDatumGetCString(child_name_datum);
                
                // Process array keys
                ArrayType *parent_key_array = DatumGetArrayTypeP(parent_key_datum);
                ArrayType *child_key_array = DatumGetArrayTypeP(child_key_datum);
                
                Datum *parent_key_elems, *child_key_elems;
                bool *parent_key_nulls, *child_key_nulls;
                int parent_key_nelems, child_key_nelems;
                
                int16 typlen;
                bool typbyval;
                char typalign;
                
                get_typlenbyvalalign(ARR_ELEMTYPE(parent_key_array), &typlen, &typbyval, &typalign);
                deconstruct_array(parent_key_array, ARR_ELEMTYPE(parent_key_array), 
                                 typlen, typbyval, typalign,
                                 &parent_key_elems, &parent_key_nulls, &parent_key_nelems);
                                 
                get_typlenbyvalalign(ARR_ELEMTYPE(child_key_array), &typlen, &typbyval, &typalign);
                deconstruct_array(child_key_array, ARR_ELEMTYPE(child_key_array), 
                                 typlen, typbyval, typalign,
                                 &child_key_elems, &child_key_nulls, &child_key_nelems);
                
                appendStringInfo(buf, " JOIN %s ON (", parent_name);
                
                for (int j = 0; j < parent_key_nelems && j < child_key_nelems; j++) {
                    if (j > 0) appendStringInfoString(buf, " AND ");
                    
                    char *parent_key = TextDatumGetCString(parent_key_elems[j]);
                    char *child_key = TextDatumGetCString(child_key_elems[j]);
                    
                    appendStringInfo(buf, "%s.%s = %s.%s", 
                                   child_name, child_key, 
                                   parent_name, parent_key);
                                   
                    pfree(parent_key);
                    pfree(child_key);
                }
                
                appendStringInfoString(buf, ")");
                
                pfree(parent_name);
                pfree(child_name);
                pfree(parent_key_elems);
                pfree(child_key_elems);
                pfree(parent_key_nulls);
                pfree(child_key_nulls);
            }
            
            if (SPI_processed > 0) {
                HeapTuple tuple = SPI_tuptable->vals[last_idx];
                bool isnull_parent, isnull_child, isnull_parent_key, isnull_child_key;
                
                Datum parent_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull_parent);
                Datum parent_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull_parent_key);
                Datum child_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 3, &isnull_child);
                Datum child_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 4, &isnull_child_key);
                
                if (!isnull_parent && !isnull_child && !isnull_parent_key && !isnull_child_key) {
                    char *parent_name = TextDatumGetCString(parent_name_datum);
                    char *child_name = TextDatumGetCString(child_name_datum);
                    
                    ArrayType *parent_key_array = DatumGetArrayTypeP(parent_key_datum);
                    ArrayType *child_key_array = DatumGetArrayTypeP(child_key_datum);
                    
                    Datum *parent_key_elems, *child_key_elems;
                    bool *parent_key_nulls, *child_key_nulls;
                    int parent_key_nelems, child_key_nelems;
                    
                    int16 typlen;
                    bool typbyval;
                    char typalign;
                    
                    get_typlenbyvalalign(ARR_ELEMTYPE(parent_key_array), &typlen, &typbyval, &typalign);
                    deconstruct_array(parent_key_array, ARR_ELEMTYPE(parent_key_array), 
                                     typlen, typbyval, typalign,
                                     &parent_key_elems, &parent_key_nulls, &parent_key_nelems);
                                     
                    get_typlenbyvalalign(ARR_ELEMTYPE(child_key_array), &typlen, &typbyval, &typalign);
                    deconstruct_array(child_key_array, ARR_ELEMTYPE(child_key_array), 
                                     typlen, typbyval, typalign,
                                     &child_key_elems, &child_key_nulls, &child_key_nelems);
                    
                    appendStringInfoString(buf, " WHERE ");
                    
                    for (int j = 0; j < parent_key_nelems && j < child_key_nelems; j++) {
                        if (j > 0) appendStringInfoString(buf, " AND ");
                        
                        char *parent_key = TextDatumGetCString(parent_key_elems[j]);
                        char *child_key = TextDatumGetCString(child_key_elems[j]);
                        
                        appendStringInfo(buf, "%s.%s = %s.%s", 
                                       child_name, child_key, 
                                       parent_name, parent_key);
                                       
                        pfree(parent_key);
                        pfree(child_key);
                    }
                    
                    // Clean up
                    pfree(parent_name);
                    pfree(child_name);
                    pfree(parent_key_elems);
                    pfree(child_key_elems);
                    pfree(parent_key_nulls);
                    pfree(child_key_nulls);
                }
            }
        } else {
            appendStringInfoString(buf, child);
            elog(WARNING, "No hierarchy data found for parent=%s, child=%s", parent, child);
        }
    }
    PG_FINALLY();
    {
        SPI_finish();
    }
    PG_END_TRY();
    
    SPI_finish();
}

Datum
pg_hier_return_one(const char *sql)
{
    int ret;
    Datum result = (Datum) NULL;
    bool is_null;
    
    if ((ret = SPI_connect()) < 0)
        elog(ERROR, "SPI_connect failed: %s", SPI_result_code_string(ret));
    
    ret = SPI_execute(sql, true, 0);
    
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        elog(ERROR, "SPI_execute failed: %s", SPI_result_code_string(ret));
    }
    
    if (SPI_processed > 0 && SPI_tuptable != NULL)  
    {
        if (SPI_tuptable->vals[0] != NULL)  
        {
            bool isnull;
            Datum val = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull);
            
            if (!isnull)
                result = datumCopy(val, false, -1);
        }
    }

    SPI_finish();

    return result;
}

/**************************************
 * Helper function to trim
 * whitespace from a string
 **************************************/
static char *
trim_whitespace(char *str)
{
    if (str == NULL)
        return NULL;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

/**************************************
 * Helper function to reorder tables
 * based on hierarchy string
 **************************************/
string_array *
reorder_tables(string_array *tables, char *hierarchy_string)
{
    string_array *result = create_string_array();

    struct table_position *positions = 
        palloc(tables->size * sizeof(struct table_position));

    for (int i = 0; i < tables->size; i++)
    {
        positions[i].original_position = i;
        char *table = tables->data[i];
        char *pos = strstr(hierarchy_string, table);
        if (pos)
            positions[i].hierarchy_position = pos - hierarchy_string;
        else
            positions[i].hierarchy_position = strlen(hierarchy_string);
    }

    qsort(positions, tables->size, sizeof(struct table_position), 
                                        compare_string_positions);

    if (tables->size)
        copy_string_array(result, tables);

    pfree(positions);

    return result;
}

static int 
compare_string_positions(const void *a, const void *b)
{
    struct table_position *posA = (struct table_position *)a;
    struct table_position *posB = (struct table_position *)b;

    return posA->hierarchy_position - posB->hierarchy_position;
}

JsonbValue
datum_to_jsonb_value(Datum value_datum, Oid value_type)
{
    JsonbValue val;
    Oid typoutput;
    bool typIsVarlena;
    char *outputstr = NULL;

    memset(&val, 0, sizeof(JsonbValue));

    switch (value_type)
    {
    case INT2OID:
    case INT4OID:
    case INT8OID:
    case FLOAT4OID:
    case FLOAT8OID:
    case NUMERICOID:
    {
        getTypeOutputInfo(value_type, &typoutput, &typIsVarlena);
        outputstr = OidOutputFunctionCall(typoutput, value_datum);
        val.type = jbvString;
        val.val.string.val = pstrdup(outputstr);
        val.val.string.len = strlen(outputstr);
        pfree(outputstr);
        break;
    }
    case BOOLOID:
        val.type = jbvBool;
        val.val.boolean = DatumGetBool(value_datum);
        break;
    case TEXTOID:
    case VARCHAROID:
    {
        text *txt = DatumGetTextPP(value_datum);
        outputstr = text_to_cstring(txt);
        val.type = jbvString;
        val.val.string.val = pstrdup(outputstr);
        val.val.string.len = strlen(outputstr);
        pfree(outputstr);
        break;
    }
    default:
        getTypeOutputInfo(value_type, &typoutput, &typIsVarlena);
        outputstr = OidOutputFunctionCall(typoutput, value_datum);
        val.type = jbvString;
        val.val.string.val = pstrdup(outputstr);
        val.val.string.len = strlen(outputstr);
        pfree(outputstr);
        break;
    }

    return val;
}

Jsonb *
get_or_create_array(Jsonb *existing_jsonb, char *key_str)
{
    JsonbValue key;
    JsonbIterator *it;
    JsonbIteratorToken r;
    JsonbValue v;
    JsonbParseState *arr_parse_state = NULL;
    bool found = false;

    key.type = jbvString;
    key.val.string.val = key_str;
    key.val.string.len = strlen(key_str);

    if (existing_jsonb)
    {
        it = JsonbIteratorInit(&existing_jsonb->root);
        r = JsonbIteratorNext(&it, &v, true);

        while ((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
        {
            if (r == WJB_KEY &&
                v.val.string.len == key.val.string.len &&
                memcmp(v.val.string.val, key.val.string.val, v.val.string.len) == 0)
            {
                found = true;

                r = JsonbIteratorNext(&it, &v, true);
                if (r == WJB_BEGIN_ARRAY)
                {
                    arr_parse_state = NULL;
                    pushJsonbValue(&arr_parse_state, WJB_BEGIN_ARRAY, NULL);

                    while ((r = JsonbIteratorNext(&it, &v, true)) != WJB_END_ARRAY)
                    {
                        if (r == WJB_ELEM)
                        {
                            pushJsonbValue(&arr_parse_state, WJB_ELEM, &v);
                        }
                    }
                    break;
                }
            }
        }
    }
    if (!found)
    {
        arr_parse_state = NULL;
        pushJsonbValue(&arr_parse_state, WJB_BEGIN_ARRAY, NULL);
    }
    if (arr_parse_state != NULL)
    {
        return JsonbValueToJsonb(pushJsonbValue(&arr_parse_state, 
                                                WJB_END_ARRAY, NULL));
    }
    else
    {
        return NULL;
    }
}

int 
find_column_index(ColumnArrayState *state, text *column_name)
{
    for (int i = 0; i < state->num_columns; i++)
    {
        text *existing = state->column_names[i];
        if (VARSIZE_ANY_EXHDR(column_name) == VARSIZE_ANY_EXHDR(existing) &&
            memcmp(VARDATA_ANY(column_name), VARDATA_ANY(existing),
                   VARSIZE_ANY_EXHDR(column_name)) == 0)
        {
            return i;
        }
    }
    return -1;
}

void 
datum_to_jsonb(Datum val, Oid val_type, JsonbValue *result)
{
    switch (val_type)
    {
    case BOOLOID:
        result->type = jbvBool;
        result->val.boolean = DatumGetBool(val);
        break;

    case INT2OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(
            DirectFunctionCall1(int2_numeric, val));
        break;

    case INT4OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(
            DirectFunctionCall1(int4_numeric, val));
        break;

    case INT8OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(
            DirectFunctionCall1(int8_numeric, val));
        break;

    case FLOAT4OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(
            DirectFunctionCall1(float4_numeric, val));
        break;

    case FLOAT8OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(
            DirectFunctionCall1(float8_numeric, val));
        break;

    case NUMERICOID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(val);
        break;

    case TEXTOID:
    case VARCHAROID:
    case BPCHAROID:
    case NAMEOID:
    {
        text *t = DatumGetTextP(val);
        result->type = jbvString;
        result->val.string.val = VARDATA_ANY(t);
        result->val.string.len = VARSIZE_ANY_EXHDR(t);
        break;
    }

    case JSONOID:
    {
        Jsonb *jb;
        jb = DatumGetJsonbP(DirectFunctionCall1(to_jsonb, val));

        JsonbContainer *container = &jb->root;
        if (JB_ROOT_IS_SCALAR(container))
        {
            JsonbValue scalar_value;
            JsonbIterator *it = JsonbIteratorInit(container);
            JsonbIteratorToken tok = JsonbIteratorNext(&it, &scalar_value, true);

            if (tok == WJB_VALUE)
            {
                *result = scalar_value;
            }
            else
            {
                result->type = jbvString;
                result->val.string.val = "[Complex JSON value]";
                result->val.string.len = 20;
            }
        }
        else
        {
            result->type = jbvBinary;
            result->val.binary.data = jb;
            result->val.binary.len = VARSIZE(jb);
        }
        break;
    }

    case JSONBOID:
    {
        Jsonb *jb = DatumGetJsonbP(val);
        JsonbContainer *container = &jb->root;

        if (JB_ROOT_IS_SCALAR(container))
        {
            JsonbValue scalar_value;
            JsonbIterator *it = JsonbIteratorInit(container);
            JsonbIteratorToken tok = JsonbIteratorNext(&it, &scalar_value, true);

            if (tok == WJB_VALUE)
            {
                *result = scalar_value;
            }
            else
            {
                result->type = jbvString;
                result->val.string.val = "[Complex JSONB value]";
                result->val.string.len = 21;
            }
        }
        else
        {
            result->type = jbvBinary;
            result->val.binary.data = jb;
            result->val.binary.len = VARSIZE(jb);
        }
        break;
    }

    default:
    {
        char *out_str;
        Oid output_function_id;
        bool is_varlena;

        getTypeOutputInfo(val_type, &output_function_id, &is_varlena);
        out_str = OidOutputFunctionCall(output_function_id, val);

        result->type = jbvString;
        result->val.string.val = out_str;
        result->val.string.len = strlen(out_str);
        break;
    }
    }
}
