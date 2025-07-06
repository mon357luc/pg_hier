#include "pg_hier_helper.h"

void parse_input(StringInfo buf, const char *input, struct string_array **tables)
{
    struct table_stack *stack = NULL;
    char *input_copy = NULL;
    char *token = NULL;
    char *next_token = NULL;
    char *saveptr = NULL;
    bool first_column = true;
    PG_TRY();
    {
        input_copy = pstrdup(input);
        *tables = create_string_array();

        appendStringInfoString(buf, "jsonb_build_object(");

        token = trim_whitespace(strtok_r(input_copy, ", \n\r\t", &saveptr));
        next_token = get_token(&saveptr);

        while (token)
        {
            elog(INFO, "Processing:\n\tCurr_token: %s\n\tNext_token: %s", token, next_token);
            if (*token == '\0')
                break;

            if (next_token && !strcmp(next_token, "{"))
            {
                add_string_to_array(*tables, token);
                if (!first_column)
                    appendStringInfoString(buf, ", ");
                appendStringInfo(buf, "'%s', json_build_object(", token);

                stack = create_table_stack_entry(token, stack);
                first_column = true;
                token = get_token(&saveptr);
                next_token = get_token(&saveptr);
            }
            else
            {
                while (token)
                {
                    elog(INFO, "Processing:\n\t\tCurr_token: %s\n\t\tNext_token: %s", token, next_token);
                    if (*token == '\0')
                        break;

                    if (next_token && !strcmp(next_token, "{"))
                        break;

                    if (token && strcmp(token, "}") == 0)
                    {
                        elog(INFO, "Close brace");
                        if (stack == NULL)
                        {
                            ereport(ERROR, (errmsg("Unmatched closing brace in input")));
                        }
                        appendStringInfoString(buf, ")");
                        pop_table_stack(&stack);
                        token = next_token;
                        next_token = get_token(&saveptr);
                        continue;
                    }

                    if (first_column)
                        first_column = false;
                    else
                        appendStringInfoString(buf, ", ");

                    appendStringInfo(buf, "'%s', array_agg(%s.%s)",
                                     token, stack->table_name, token);
                    token = next_token;
                    next_token = get_token(&saveptr);
                }
            }
        }
        appendStringInfoString(buf, ")");
        elog(INFO, "Buffer: %s", buf->data);

        if (stack)
            free_table_stack(&stack);

        if (input_copy)
            pfree(input_copy);
        input_copy = NULL;
    }
    PG_CATCH();
    {
        if (stack)
            free_table_stack(&stack);
        if (input_copy != NULL)
            pfree(input_copy);
        PG_RE_THROW();
    }
    PG_END_TRY();
}

char *
get_token(char **saveptr)
{
    // char *new_token = NULL;
    if (!saveptr)
        return NULL;
    // new_token = trim_whitespace(strtok_r(NULL, ", \n\r\t", saveptr));
    // return new_token;
    return trim_whitespace(strtok_r(NULL, ", \n\r\t", saveptr));
}

/**************************************
 * Helper function to trim
 * whitespace from a string
 **************************************/
char *
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

int pg_hier_find_hier(struct string_array *tables)
{
    if (tables == NULL || tables->size < 2)
    {
        ereport(ERROR, (errmsg("Name path elements must contain at least two elements")));
    }

    StringInfoData query;
    initStringInfo(&query);
    appendStringInfo(&query, "SELECT id, table_path FROM pg_hier_header WHERE ");
    for (int i = 0; i < tables->size; i++)
    {
        if (i > 0)
            appendStringInfoString(&query, " AND ");
        appendStringInfo(&query, "table_path LIKE '%%%s%%'", tables->data[i]);
    }

    int ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
    {
        elog(ERROR, "SPI_connect failed: %d", ret);
    }

    ret = SPI_execute(query.data, true, 0);

    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        elog(ERROR, "SPI_execute failed: %d", ret);
    }

    uint64 hier_id = 0;
    char *hierarchy_string = NULL;

    if (SPI_processed > 0)
    {
        HeapTuple tuple = SPI_tuptable->vals[0];
        bool isnull;
        Datum id_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull);
        hier_id = DatumGetUInt64(id_datum);
        Datum path_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull);
        hierarchy_string = pstrdup(TextDatumGetCString(path_datum));
    }
    SPI_finish();
    pfree(query.data);
    if (hierarchy_string != NULL)
    {
        reorder_tables(tables, hierarchy_string);
        pfree(hierarchy_string);
    }
    return hier_id;
}

/**************************************
 * Helper function to reorder tables
 * based on hierarchy string
 **************************************/
void reorder_tables(struct string_array *tables, char *hierarchy_string)
{
    int *original_positions = palloc(tables->size * sizeof(int));
    for (int i = 0; i < tables->size; i++)
    {
        original_positions[i] = i;
    }

    struct table_position
    {
        int original_position;
        int hierarchy_position;
    } *positions = palloc(tables->size * sizeof(struct table_position));

    for (int i = 0; i < tables->size; i++)
    {
        positions[i].original_position = i;
        char *table = tables->data[i];
        char *pos = strstr(hierarchy_string, table);
        if (pos)
        {
            positions[i].hierarchy_position = pos - hierarchy_string;
        }
        else
        {
            positions[i].hierarchy_position = strlen(hierarchy_string);
        }
    }

    qsort(positions, tables->size, sizeof(struct table_position), compare_string_positions);

    char **reordered_data = palloc(tables->size * sizeof(char *));

    for (int i = 0; i < tables->size; i++)
    {
        reordered_data[i] = pstrdup(tables->data[positions[i].original_position]);
    }

    for (int i = 0; i < tables->size; ++i)
    {
        pfree(tables->data[i]);
    }
    pfree(tables->data);

    tables->data = reordered_data;

    pfree(positions);
    pfree(original_positions);
}

static int compare_string_positions(const void *a, const void *b)
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
        return JsonbValueToJsonb(pushJsonbValue(&arr_parse_state, WJB_END_ARRAY, NULL));
    }
    else
    {
        return NULL;
    }
}

int find_column_index(ColumnArrayState *state, text *column_name)
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

void datum_to_jsonb(Datum val, Oid val_type, JsonbValue *result)
{
    switch (val_type)
    {
    case BOOLOID:
        result->type = jbvBool;
        result->val.boolean = DatumGetBool(val);
        break;

    case INT2OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(DirectFunctionCall1(int2_numeric, val));
        break;

    case INT4OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(DirectFunctionCall1(int4_numeric, val));
        break;

    case INT8OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(DirectFunctionCall1(int8_numeric, val));
        break;

    case FLOAT4OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(DirectFunctionCall1(float4_numeric, val));
        break;

    case FLOAT8OID:
        result->type = jbvNumeric;
        result->val.numeric = DatumGetNumeric(DirectFunctionCall1(float8_numeric, val));
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
