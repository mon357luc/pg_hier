#include "pg_hier_helper.h"

/**************************************
 * Input parsing function
 *
 * This function parses the input
 * based on the next token
 * and builds the SQL query
 **************************************/
void 
parse_input(StringInfo buf, const char *input, string_array **tables)
{
    table_stack *stack = NULL;
    hier_header *hh = NULL;
    char *where_clause = NULL;
    char *parent_table = NULL;
    char *child_table = NULL;
    char *input_copy = NULL;
    char *token = NULL;
    char *next_token = NULL;
    char *saveptr = NULL;

    MemoryContext old_context = MemoryContextSwitchTo(TopMemoryContext);

    StringInfoData rel_buf;
    initStringInfo(&rel_buf);

    StringInfo cur_buf = buf;

    PG_TRY();
    {
        *tables = create_string_array();
        hh = CREATE_HIER_HEADER();
        input_copy = pstrdup(input);

        // get first token (table name) and { or (
        token = GET_TOKEN(input_copy, &saveptr);
        next_token = GET_TOKEN(&saveptr);

        if (!token || *token == '\0')
            pg_hier_error_empty_input();

        do
        {
            switch (*next_token)
            {
            case '(': // Begin where clause
                child_table = pstrdup(token);
                add_string_to_array(*tables, child_table);
                stack = create_table_stack_entry(child_table, stack);



                token = next_token;
                next_token = GET_TOKEN(&saveptr);
                cur_buf = &stack->where_condition;

                pg_hier_where_clause(cur_buf, next_token, &saveptr);
                
                // After pg_hier_where_clause, get the closing ')'
                next_token = GET_TOKEN(&saveptr);
                if (!next_token || strcmp(next_token, ")") != 0) 
                {
                    pg_hier_error_expected_token(")", next_token, "where clause");
                }
                
                cur_buf = buf;
                break;

            case ')': // End where clause
                break;

            case '{': // Begin subquery
                if (strcmp(token, ")"))
                {
                    child_table = pstrdup(token);
                    add_string_to_array(*tables, child_table);
                    stack = create_table_stack_entry(child_table, stack);
                }
                else
                {
                    child_table = pstrdup(stack->table_name);
                }

                if (stack->next && !stack->next->first_column)
                    appendStringInfoString(cur_buf, ", ");
                appendStringInfo(cur_buf, "'%s', (SELECT jsonb_agg(jsonb_build_object(", child_table);

                pfree(child_table);
                child_table = NULL;
                break;

            case '}':
                if (!is_special_char(*token))
                {
                    if (stack && !stack->first_column)
                        appendStringInfoString(cur_buf, ", ");
                    else
                        stack->first_column = false;
                    appendStringInfo(cur_buf, "'%s', %s.%s", token, stack->table_name, token);
                }
                if (stack == NULL)
                    pg_hier_error_unmatched_braces();

                if (stack->where_condition.len > 0)
                    where_clause = pstrdup(stack->where_condition.data);
                child_table = pop_table_stack(&stack);
                parent_table = peek_table_stack(&stack);

                if (parent_table)
                {
                    pg_hier_get_hier(*tables, hh);
                    pg_hier_from_clause(&rel_buf, hh, parent_table, child_table);
                    appendStringInfo(cur_buf, ")) FROM %s", rel_buf.data);
                    resetStringInfo(&rel_buf);

                    if (where_clause)
                    {
                        appendStringInfo(cur_buf, " AND %s", where_clause);
                        pfree(where_clause);
                        where_clause = NULL;
                    }
                    appendStringInfoString(cur_buf, ")");
                    pfree(parent_table);
                    parent_table = NULL;
                }
                else
                {
                    appendStringInfo(cur_buf, ")) FROM %s", child_table);
                    if (where_clause)
                    {
                        appendStringInfo(cur_buf, " WHERE %s", where_clause);
                        pfree(where_clause);
                        where_clause = NULL;
                    }
                }
                pfree(child_table);
                child_table = NULL;
                break;

            case ',': // Next column
            case '[':
            case ']':
            case ';':
            default:
                if (!stack)
                {
                    break;
                }
                else if (is_special_char(*token))
                {
                    break;
                }
                else if (!stack->first_column)
                {
                    appendStringInfo(cur_buf, ", '%s', %s.%s", token, stack->table_name, token);
                }
                else
                {
                    appendStringInfo(cur_buf, "'%s', %s.%s", token, stack->table_name, token);
                    stack->first_column = false;
                }
                break;
            }
            token = next_token;
            next_token = GET_TOKEN(&saveptr);
        } while (next_token && *next_token != '\0');
        appendStringInfoString(cur_buf, ")));");
    }
    PG_FINALLY();
    {
        MemoryContextSwitchTo(old_context);
    }
    PG_END_TRY();
}

static char *
first_token(const char *input, char **saveptr)
{
    if (!input)
        return NULL;
    *saveptr = (char *)input;
    return get_next_token(saveptr);
}

static char *
get_next_token(char **saveptr)
{
    char *s = *saveptr;
    if (!s)
        return NULL;

    while (*s && isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
    {
        *saveptr = s;
        return NULL;
    }

    if (is_special_char(*s))
    {
        char *tok = s;
        s++;
        *saveptr = s;
        char *result = palloc(2);
        result[0] = *tok;
        result[1] = '\0';
        return result;
    }

    // Handle regular tokens
    char *start = s;
    while (*s && !isspace((unsigned char)*s) && !is_special_char(*s))
        s++;

    size_t len = s - start;
    char *result = palloc(len + 1);
    strncpy(result, start, len);
    result[len] = '\0';
    *saveptr = s;

    return result;
}

static inline bool
is_special_char(char c)
{
    return (c == '{' || c == '}' || c == '[' || c == ']' || c == '(' || c == ')' || c == ',' || c == ';' || c == ':');
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

void 
pg_hier_get_hier(string_array *tables, hier_header *hh)
{
    // Attempts to check 'cache,' if hh is null or
    // newest table isn't in hh, get new hh
    if (!hh || hh->deepest_nest < 0)
        pg_hier_find_hier(tables, hh);
    else
        for (int i = hh->deepest_nest; i < tables->size; i++)
            if (!strstr(hh->hier, tables->data[i]))
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
        pg_hier_error_insufficient_path_elements();

    initStringInfo(&query);
    appendStringInfo(&query, "SELECT id, table_path FROM pg_hier_header WHERE ");
    for (int i = 0; i < tables->size; i++)
        appendStringInfo(&query,
                         (
                             (i > 0) ? " AND table_path LIKE '%%%s%%'" : "table_path LIKE '%%%s%%'"),
                         tables->data[i]);

    PG_TRY();
    {
        if ((ret = SPI_connect()) != SPI_OK_CONNECT)
            pg_hier_error_spi_connect(ret);

        if ((ret = SPI_execute(query.data, true, 0)) != SPI_OK_SELECT)
            pg_hier_error_spi_execute(ret);

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
pg_hier_where_clause(StringInfo buf, char *token, char **saveptr)
{
    int lvl = 0;
    char *next_token = NULL;
    MemoryContext old_context = MemoryContextSwitchTo(TopMemoryContext);
    
    PG_TRY();
    {    
        if (pg_strcasecmp(token, "where"))
            pg_hier_error_expected_where(token);
            
        next_token = GET_TOKEN(saveptr);
        if (strcmp(next_token, "{"))
            pg_hier_error_expected_token("{", next_token, "'where'");
        
        lvl++;
        
        next_token = GET_TOKEN(saveptr);
        
        if (!strcmp(next_token, "_and") || !strcmp(next_token, "and")) 
        {
            _and_operator(buf, saveptr, &lvl);
        }
        else if (!strcmp(next_token, "_or") || !strcmp(next_token, "or")) 
        {
            _or_operator(buf, saveptr, &lvl);
        }
        else 
        {
            _parse_condition(buf, next_token, saveptr, &lvl);
        }

        next_token = GET_TOKEN(saveptr);
        if (strcmp(next_token, "}"))
            pg_hier_error_expected_token("}", next_token, "where clause");
        lvl--;

        if (lvl)
            pg_hier_error_mismatched_braces();
    }
    PG_FINALLY();
    {
        MemoryContextSwitchTo(old_context);
    }
    PG_END_TRY();
}

static void 
_and_operator(StringInfo buf, char **saveptr, int *lvl)
{
    char *next_token;
    
    next_token = GET_TOKEN(saveptr);
    if (strcmp(next_token, "{"))
        pg_hier_error_expected_token("{", next_token, "'_and'");
    
    (*lvl)++;
    
    appendStringInfo(buf, "(");
    
    bool first_condition = true;
    next_token = GET_TOKEN(saveptr);
    
    while (next_token && strcmp(next_token, "}")) 
    {
        if (!first_condition)
            appendStringInfo(buf, " AND ");
        else
            first_condition = false;

        // Check if it's a nested operator
        if (!strcmp(next_token, "_and") || !strcmp(next_token, "and"))
        {
            _and_operator(buf, saveptr, lvl);
        }
        else if (!strcmp(next_token, "_or") || !strcmp(next_token, "or"))
        {
            _or_operator(buf, saveptr, lvl);
        }
        else
        {
            // It's a field name, parse the condition
            _parse_condition(buf, next_token, saveptr, lvl);
        }
        
        next_token = GET_TOKEN(saveptr);
    }
    
    appendStringInfo(buf, ")");
    (*lvl)--;
}

static void 
_or_operator(StringInfo buf, char **saveptr, int *lvl)
{
    char *next_token;
    
    next_token = GET_TOKEN(saveptr);
    if (strcmp(next_token, "{") != 0)
        pg_hier_error_expected_token("{", next_token, "'_or'");

    (*lvl)++;

    appendStringInfo(buf, "(");

    bool first_condition = true;
    next_token = GET_TOKEN(saveptr);

    while (next_token && strcmp(next_token, "}") != 0)
    {
        if (!first_condition)
            appendStringInfo(buf, " OR ");
        else
            first_condition = false;

        // Check if it's a nested operator
        if (!strcmp(next_token, "_and") || !strcmp(next_token, "and"))
        {
            _and_operator(buf, saveptr, lvl);
        }
        else if (!strcmp(next_token, "_or") || !strcmp(next_token, "or"))
        {
            _or_operator(buf, saveptr, lvl);
        }
        else
        {
            // It's a field name, parse the condition
            _parse_condition(buf, next_token, saveptr, lvl);
        }

        next_token = GET_TOKEN(saveptr);
    }

    appendStringInfo(buf, ")");
    (*lvl)--;
}

static void
_parse_condition(StringInfo buf, char *field_name, char **saveptr, int *lvl)
{
    char *next_token = GET_TOKEN(saveptr);

    if (!strcmp(next_token, ":"))
        next_token = GET_TOKEN(saveptr);
    
    if (strcmp(next_token, "{"))
        pg_hier_error_expected_token("{", next_token, field_name);
    
    (*lvl)++;
    
    char *operator = GET_TOKEN(saveptr);
    
    next_token = GET_TOKEN(saveptr);
    if (!strcmp(next_token, ":"))
        next_token = GET_TOKEN(saveptr);
    else
        pg_hier_error_expected_token(":", next_token, operator);

    if (next_token == NULL)
        pg_hier_error_unexpected_end(operator);
    
    if (!strcmp(operator, "_eq")) 
    {
        appendStringInfo(buf, "%s = %s", field_name, next_token);
    } 
    else if (!strcmp(operator, "_like")) 
    {
        appendStringInfo(buf, "%s LIKE %s", field_name, next_token);
    }
    else if (!strcmp(operator, "_gt")) 
    {
        appendStringInfo(buf, "%s > %s", field_name, next_token);
    }
    else if (!strcmp(operator, "_lt")) 
    {
        appendStringInfo(buf, "%s < %s", field_name, next_token);
    }
    else if (!strcmp(operator, "_in")) 
    {
        appendStringInfo(buf, "%s IN (%s)", field_name, next_token);
    } 
    else if (!strcmp(operator, "_not_in")) 
    {
        appendStringInfo(buf, "%s NOT IN (%s)", field_name, next_token);
    } 
    else if (!strcmp(operator, "_is_null")) 
    {
        appendStringInfo(buf, "%s IS NULL", field_name);
    } 
    else if (!strcmp(operator, "_is_not_null")) 
    {
        appendStringInfo(buf, "%s IS NOT NULL", field_name);
    }
    else if (!strcmp(operator, "_between")) 
    {
        char *lower_bound = GET_TOKEN(saveptr);
        char *upper_bound = GET_TOKEN(saveptr);
        if (lower_bound == NULL || upper_bound == NULL)
            pg_hier_error_invalid_between_values(lower_bound, upper_bound);
        appendStringInfo(buf, "%s BETWEEN %s AND %s", field_name, lower_bound, upper_bound);
    }
    else if (!strcmp(operator, "_not_between")) 
    {
        char *lower_bound = GET_TOKEN(saveptr);
        char *upper_bound = GET_TOKEN(saveptr);
        if (lower_bound == NULL || upper_bound == NULL)
            pg_hier_error_invalid_between_values(lower_bound, upper_bound);
        appendStringInfo(buf, "%s NOT BETWEEN %s AND %s", field_name, lower_bound, upper_bound);
    }
    else if (!strcmp(operator, "_exists")) 
    {
        appendStringInfo(buf, "EXISTS (SELECT 1 FROM %s WHERE %s)", next_token, field_name);
    } 
    else if (!strcmp(operator, "_not_exists")) 
    {
        appendStringInfo(buf, "NOT EXISTS (SELECT 1 FROM %s WHERE %s)", next_token, field_name);
    } 
    else if (!strcmp(operator, "_is_true")) 
    {
        appendStringInfo(buf, "%s IS TRUE", field_name);
    } 
    else if (!strcmp(operator, "_is_false")) 
    {
        appendStringInfo(buf, "%s IS FALSE", field_name);
    }
    else 
    {
        pg_hier_error_unknown_operator(operator);
    }
    
    next_token = GET_TOKEN(saveptr);
    if (strcmp(next_token, "}"))
        pg_hier_error_expected_token("}", next_token, "condition");

    (*lvl)--;
}

void 
pg_hier_from_clause(StringInfo buf, hier_header *hh, char *parent, char *child)
{
    int ret;

    MemoryContext old_context = MemoryContextSwitchTo(TopMemoryContext);

    if (!hh || !parent || !child)
    {
        pg_hier_warning_missing_params();
        if (child)
            appendStringInfoString(buf, child);
        return;
    }

    PG_TRY();
    {
        if ((ret = SPI_connect()) != SPI_OK_CONNECT)
            pg_hier_error_spi_connect(ret);

        text *parent_text = parent ? cstring_to_text(parent) : NULL;
        text *child_text = child ? cstring_to_text(child) : NULL;

        Oid argtypes[3] = {INT4OID, TEXTOID, TEXTOID};
        Datum values[3] = {
            Int32GetDatum(hh->hier_id),
            PointerGetDatum(parent_text),
            PointerGetDatum(child_text)};

        ret = SPI_execute_with_args(
            PG_HIER_SQL_GET_HIER_BY_ID,
            3, argtypes, values, NULL, true, 0);

        if (parent_text)
            pfree(parent_text);
        if (child_text)
            pfree(child_text);

        if (ret != SPI_OK_SELECT)
            pg_hier_error_spi_execute(ret);

        if (SPI_processed > 0)
        {
            bool first_join = true;
            HeapTuple last_tuple = NULL;
            int last_idx = SPI_processed - 1;

            appendStringInfoString(buf, child);

            for (int i = 0; i < last_idx; i++)
            {
                HeapTuple tuple = SPI_tuptable->vals[i];
                bool isnull_parent, isnull_child, isnull_parent_key, isnull_child_key;

                Datum parent_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull_parent);
                Datum parent_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull_parent_key);
                Datum child_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 3, &isnull_child);
                Datum child_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 4, &isnull_child_key);

                if (isnull_parent || isnull_child || isnull_parent_key || isnull_child_key)
                {
                    pg_hier_warning_null_values(i);
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

                for (int j = 0; j < parent_key_nelems && j < child_key_nelems; j++)
                {
                    if (j > 0)
                        appendStringInfoString(buf, " AND ");

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

            if (SPI_processed > 0)
            {
                HeapTuple tuple = SPI_tuptable->vals[last_idx];
                bool isnull_parent, isnull_child, isnull_parent_key, isnull_child_key;

                Datum parent_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull_parent);
                Datum parent_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull_parent_key);
                Datum child_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 3, &isnull_child);
                Datum child_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 4, &isnull_child_key);

                if (!isnull_parent && !isnull_child && !isnull_parent_key && !isnull_child_key)
                {
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

                    for (int j = 0; j < parent_key_nelems && j < child_key_nelems; j++)
                    {
                        if (j > 0)
                            appendStringInfoString(buf, " AND ");

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
        }
        else
        {
            appendStringInfoString(buf, child);
            pg_hier_error_no_hierarchy_data(parent, child);
        }
    }
    PG_FINALLY();
    {
        SPI_finish();
        MemoryContextSwitchTo(old_context);
    }
    PG_END_TRY();
}

Datum 
pg_hier_return_one(const char *sql)
{
    int ret;
    Datum result = (Datum)NULL;
    bool is_null;

    if ((ret = SPI_connect()) < 0)
        pg_hier_error_spi_connect(ret);

    ret = SPI_execute(sql, true, 0);

    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        pg_hier_error_spi_execute(ret);
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
