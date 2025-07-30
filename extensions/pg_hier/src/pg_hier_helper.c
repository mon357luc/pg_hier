#include "pg_hier_helper.h"

/**************************************
 * Security validation functions
 **************************************/
void
validate_input_security(const char *input)
{
    const char *s = input;
    
    if (!input)
        return;
        
    while (*s)
    {
        // Check for SQL line comments
        if (*s == '-' && *(s + 1) == '-')
        {
            ereport(ERROR, 
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("SQL line comments (--) are not allowed in pg_hier input"),
                     errhint("Remove any comment syntax from your hierarchical query")));
        }
        
        // Check for SQL block comments
        if (*s == '/' && *(s + 1) == '*')
        {
            ereport(ERROR, 
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("SQL block comments (/* */) are not allowed in pg_hier input"),
                     errhint("Remove any comment syntax from your hierarchical query")));
        }
        
        // Check for other potentially dangerous patterns
        if (*s == ';' && *(s + 1) && !isspace(*(s + 1)))
        {
            ereport(ERROR, 
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("Multiple SQL statements are not allowed in pg_hier input"),
                     errhint("Use only single hierarchical query syntax")));
        }
        
        s++;
    }
}

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
    table_stack *child_node = NULL;
    hier_header *hh = NULL;
    char *parent_table = NULL;
    char *child_table = NULL;
    char *where_clause = NULL;
    char *input_copy = NULL;
    char *token = NULL;
    char *next_token = NULL;
    char *saveptr = NULL;

    MemoryContext old_context = MemoryContextSwitchTo(TopMemoryContext);

    StringInfoData rel_buf;
    StringInfo cur_buf;
    
    initStringInfo(&rel_buf);
    cur_buf = buf;

    PG_TRY();
    {
        validate_input_security(input);
        
        *tables = create_string_array();
        hh = CREATE_HIER_HEADER();
        input_copy = pstrdup(input);

        token = GET_TOKEN(input_copy, &saveptr);
        next_token = GET_TOKEN(&saveptr);

        if (!token)
            return;
        else if (!next_token)
            pg_hier_error_unexpected_end(token);
        else if (!strcmp(token, "{"))
            pg_hier_error_expected_token("[table name]", token, "input");

        do
        {
            switch (*next_token)
            {
            case '(': // Begin where clause
                child_table = pstrdup(token);
                VALIDATE_IDENTIFIER(child_table, "table name in where clause");
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
                    VALIDATE_IDENTIFIER(child_table, "table name");
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
                    char *escaped_column;
                    char *escaped_table;
                    
                    if (stack && !stack->first_column)
                        appendStringInfoString(cur_buf, ", ");
                    else
                        stack->first_column = false;
                    
                    VALIDATE_IDENTIFIER(token, "column name");
                    escaped_column = ESCAPE_IDENTIFIER(token);
                    escaped_table = ESCAPE_IDENTIFIER(stack->table_name);
                    appendStringInfo(cur_buf, "'%s', %s.%s", token, escaped_table, escaped_column);
                    pfree(escaped_column);
                    pfree(escaped_table);
                } 
                else if (!strcmp(token, "{"))
                {
                    pg_hier_error_empty_col_list();
                }

                if (!stack)
                    pg_hier_error_unmatched_braces();

                child_node = pop_table_stack(&stack);
                parent_table = peek_table_stack(&stack);

                if (child_node->where_condition.len > 0)
                    where_clause = pstrdup(child_node->where_condition.data);
                

                if (parent_table)
                {
                    pg_hier_get_hier(*tables, hh);
                    pg_hier_from_clause(&rel_buf, hh, parent_table, child_node->table_name);
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
                    char *escaped_table = ESCAPE_IDENTIFIER(child_node->table_name);
                    appendStringInfo(cur_buf, ")) FROM %s", escaped_table);
                    if (where_clause)
                    {
                        appendStringInfo(cur_buf, " WHERE %s", where_clause);
                        pfree(where_clause);
                        where_clause = NULL;
                    }
                    pfree(escaped_table);
                }
                free_table_stack(&child_node);
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
                    char *escaped_column;
                    char *escaped_table;
                    
                    VALIDATE_IDENTIFIER(token, "column name");
                    escaped_column = ESCAPE_IDENTIFIER(token);
                    escaped_table = ESCAPE_IDENTIFIER(stack->table_name);
                    appendStringInfo(cur_buf, ", '%s', %s.%s", token, escaped_table, escaped_column);
                    pfree(escaped_column);
                    pfree(escaped_table);
                }
                else
                {
                    char *escaped_column;
                    char *escaped_table;
                    
                    VALIDATE_IDENTIFIER(token, "column name");
                    escaped_column = ESCAPE_IDENTIFIER(token);
                    escaped_table = ESCAPE_IDENTIFIER(stack->table_name);
                    appendStringInfo(cur_buf, "'%s', %s.%s", token, escaped_table, escaped_column);
                    stack->first_column = false;
                    pfree(escaped_column);
                    pfree(escaped_table);
                }
                break;
            }
            token = next_token;
            next_token = GET_TOKEN(&saveptr);
        } while (next_token && *next_token != '\0');

        if (stack)
            pg_hier_error_unmatched_braces();
        appendStringInfoString(cur_buf, ")));");
    }
    PG_FINALLY();
    {
        MemoryContextSwitchTo(old_context);
    }
    PG_END_TRY();

}

char *
first_token(const char *input, char **saveptr)
{
    if (!input)
        return NULL;
    *saveptr = (char *)input;
    return get_next_token(saveptr);
}

char *
get_next_token(char **saveptr)
{
    char *s;
    char *result;
    char *start;
    size_t len;
    
    s = *saveptr;
    if (!s)
        return NULL;

    while (*s && isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
    {
        *saveptr = s;
        return NULL;
    }

    // Security check: Detect SQL comments and reject them
    if (*s == '-' && *(s + 1) == '-')
    {
        ereport(ERROR, 
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("SQL line comments (--) are not allowed in pg_hier input"),
                 errhint("Remove any comment syntax from your hierarchical query")));
    }
    
    if (*s == '/' && *(s + 1) == '*')
    {
        ereport(ERROR, 
                (errcode(ERRCODE_SYNTAX_ERROR),
                 errmsg("SQL block comments (/* */) are not allowed in pg_hier input"),
                 errhint("Remove any comment syntax from your hierarchical query")));
    }

    if (is_special_char(*s))
    {
        char *tok = s;
        s++;
        *saveptr = s;
        result = palloc(2);
        result[0] = *tok;
        result[1] = '\0';
        return result;
    }

    // Handle regular tokens
    start = s;
    while (*s && !isspace((unsigned char)*s) && !is_special_char(*s))
    {
        // Additional security check within tokens for embedded comments
        if (*s == '-' && *(s + 1) == '-')
        {
            ereport(ERROR, 
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("SQL line comments (--) are not allowed in pg_hier input"),
                     errhint("Remove any comment syntax from your hierarchical query")));
        }
        
        if (*s == '/' && *(s + 1) == '*')
        {
            ereport(ERROR, 
                    (errcode(ERRCODE_SYNTAX_ERROR),
                     errmsg("SQL block comments (/* */) are not allowed in pg_hier input"),
                     errhint("Remove any comment syntax from your hierarchical query")));
        }
        s++;
    }

    len = s - start;
    result = palloc(len + 1);
    strncpy(result, start, len);
    result[len] = '\0';

    if (!pg_strcasecmp(result, "DROP") || !pg_strcasecmp(result, "DELETE") ||
        !pg_strcasecmp(result, "UPDATE") || !pg_strcasecmp(result, "INSERT") ||
        !pg_strcasecmp(result, "CREATE") || !pg_strcasecmp(result, "ALTER") ||
        !pg_strcasecmp(result, "TRUNCATE") || !pg_strcasecmp(result, "EXECUTE"))
    {
        PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT,
            "Only SELECT statements are allowed in pg_hier_format");
    }

    *saveptr = s;

    return result;
}

bool
is_special_char(char c)
{
    return (c == '{' || c == '}' || c == '[' || c == ']' || c == '(' || c == ')' || c == ',' || c == ';' || c == ':');
}

/**************************************
 * Helper function to trim
 * whitespace from a string
 **************************************/
char *
trim_whitespace(char *str)
{
    char *end;
    
    if (str == NULL)
        return NULL;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
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
    char *hierarchy_string = NULL;
    StringInfoData query;
    uint64 hier_id = 0;
    int ret;
    char *sanitized_pattern;

    if (tables == NULL || tables->size < 2)
        pg_hier_error_insufficient_path_elements();

    initStringInfo(&query);
    appendStringInfo(&query, "SELECT id, table_path FROM pg_hier_header WHERE ");
    for (int i = 0; i < tables->size; i++)
    {
        VALIDATE_IDENTIFIER(tables->data[i], "table name in hierarchy search");
        sanitized_pattern = pg_hier_sanitize_like_pattern(tables->data[i]);
        appendStringInfo(&query,
                         (i > 0) ? " AND table_path LIKE %s" : "table_path LIKE %s",
                         sanitized_pattern);
        pfree(sanitized_pattern);
    }

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

void 
_and_operator(StringInfo buf, char **saveptr, int *lvl)
{
    char *next_token;
    bool first_condition;
    
    next_token = GET_TOKEN(saveptr);
    if (strcmp(next_token, "{"))
        pg_hier_error_expected_token("{", next_token, "'_and'");
    
    (*lvl)++;
    
    appendStringInfo(buf, "(");
    
    first_condition = true;
    next_token = GET_TOKEN(saveptr);
    
    while (next_token && strcmp(next_token, "}")) 
    {
        if (!first_condition)
            appendStringInfo(buf, " AND ");
        else
            first_condition = false;

        if (!strcmp(next_token, "_and") || !strcmp(next_token, "and"))
            _and_operator(buf, saveptr, lvl);
        else if (!strcmp(next_token, "_or") || !strcmp(next_token, "or"))
            _or_operator(buf, saveptr, lvl);
        else
            _parse_condition(buf, next_token, saveptr, lvl);
        
        next_token = GET_TOKEN(saveptr);
    }
    
    appendStringInfo(buf, ")");
    (*lvl)--;
}

void 
_or_operator(StringInfo buf, char **saveptr, int *lvl)
{
    char *next_token;
    bool first_condition;
    
    next_token = GET_TOKEN(saveptr);
    if (strcmp(next_token, "{") != 0)
        pg_hier_error_expected_token("{", next_token, "'_or'");

    (*lvl)++;

    appendStringInfo(buf, "(");

    first_condition = true;
    next_token = GET_TOKEN(saveptr);

    while (next_token && strcmp(next_token, "}") != 0)
    {
        if (!first_condition)
            appendStringInfo(buf, " OR ");
        else
            first_condition = false;

        if (!strcmp(next_token, "_and") || !strcmp(next_token, "and"))
            _and_operator(buf, saveptr, lvl);
        else if (!strcmp(next_token, "_or") || !strcmp(next_token, "or"))
            _or_operator(buf, saveptr, lvl);
        else
            _parse_condition(buf, next_token, saveptr, lvl);

        next_token = GET_TOKEN(saveptr);
    }

    appendStringInfo(buf, ")");
    (*lvl)--;
}

void
_parse_condition(StringInfo buf, char *field_name, char **saveptr, int *lvl)
{
    char *next_token;
    char *operator;
    char *escaped_field;
    char *escaped_value;
    
    // Validate field name to prevent injection
    VALIDATE_IDENTIFIER(field_name, "field name");
    escaped_field = ESCAPE_IDENTIFIER(field_name);
    
    next_token = GET_TOKEN(saveptr);

    if (!strcmp(next_token, ":"))
        next_token = GET_TOKEN(saveptr);
    
    if (strcmp(next_token, "{"))
        pg_hier_error_expected_token("{", next_token, field_name);
    
    (*lvl)++;
    
    operator = GET_TOKEN(saveptr);
    
    next_token = GET_TOKEN(saveptr);
    if (!strcmp(next_token, ":"))
        next_token = GET_TOKEN(saveptr);
    else
        pg_hier_error_expected_token(":", next_token, operator);

    if (next_token == NULL)
        pg_hier_error_unexpected_end(operator);
    else if (is_special_char(*next_token))
        pg_hier_error_invalid_condition_value(next_token, operator);
    
    if (!strcmp(operator, "_eq")) 
    {
        escaped_value = ESCAPE_LITERAL(next_token);
        appendStringInfo(buf, "%s = %s", escaped_field, escaped_value);
        pfree(escaped_value);
    } 
    else if (!strcmp(operator, "_like")) 
    {
        escaped_value = ESCAPE_LITERAL(next_token);
        appendStringInfo(buf, "%s LIKE %s", escaped_field, escaped_value);
        pfree(escaped_value);
    }
    else if (!strcmp(operator, "_gt")) 
    {
        escaped_value = ESCAPE_LITERAL(next_token);
        appendStringInfo(buf, "%s > %s", escaped_field, escaped_value);
        pfree(escaped_value);
    }
    else if (!strcmp(operator, "_lt")) 
    {
        escaped_value = ESCAPE_LITERAL(next_token);
        appendStringInfo(buf, "%s < %s", escaped_field, escaped_value);
        pfree(escaped_value);
    }
    else if (!strcmp(operator, "_in")) 
    {
        // For IN operator, we need to validate that it's a proper list
        // For now, we'll escape it as a literal and trust PostgreSQL's parser
        escaped_value = ESCAPE_LITERAL(next_token);
        appendStringInfo(buf, "%s IN (%s)", escaped_field, escaped_value);
        pfree(escaped_value);
    } 
    else if (!strcmp(operator, "_not_in")) 
    {
        escaped_value = ESCAPE_LITERAL(next_token);
        appendStringInfo(buf, "%s NOT IN (%s)", escaped_field, escaped_value);
        pfree(escaped_value);
    } 
    else if (!strcmp(operator, "_is_null")) 
    {
        appendStringInfo(buf, "%s IS NULL", escaped_field);
    } 
    else if (!strcmp(operator, "_is_not_null")) 
    {
        appendStringInfo(buf, "%s IS NOT NULL", escaped_field);
    }
    else if (!strcmp(operator, "_between")) 
    {
        char *escaped_lower;
        char *escaped_upper;
        char *lower_bound = GET_TOKEN(saveptr);
        char *upper_bound = GET_TOKEN(saveptr);
        if (lower_bound == NULL || upper_bound == NULL)
            pg_hier_error_invalid_between_values(lower_bound, upper_bound);
        
        escaped_lower = ESCAPE_LITERAL(lower_bound);
        escaped_upper = ESCAPE_LITERAL(upper_bound);
        appendStringInfo(buf, "%s BETWEEN %s AND %s", escaped_field, escaped_lower, escaped_upper);
        pfree(escaped_lower);
        pfree(escaped_upper);
    }
    else if (!strcmp(operator, "_not_between")) 
    {
        char *escaped_lower;
        char *escaped_upper;
        char *lower_bound = GET_TOKEN(saveptr);
        char *upper_bound = GET_TOKEN(saveptr);
        if (lower_bound == NULL || upper_bound == NULL)
            pg_hier_error_invalid_between_values(lower_bound, upper_bound);
        
        escaped_lower = ESCAPE_LITERAL(lower_bound);
        escaped_upper = ESCAPE_LITERAL(upper_bound);
        appendStringInfo(buf, "%s NOT BETWEEN %s AND %s", escaped_field, escaped_lower, escaped_upper);
        pfree(escaped_lower);
        pfree(escaped_upper);
    }
    else if (!strcmp(operator, "_exists")) 
    {
        char *escaped_table;
        VALIDATE_IDENTIFIER(next_token, "EXISTS table name");
        escaped_table = ESCAPE_IDENTIFIER(next_token);
        appendStringInfo(buf, "EXISTS (SELECT 1 FROM %s WHERE %s IS NOT NULL)", escaped_table, escaped_field);
        pfree(escaped_table);
    } 
    else if (!strcmp(operator, "_not_exists")) 
    {
        char *escaped_table;
        VALIDATE_IDENTIFIER(next_token, "NOT EXISTS table name");
        escaped_table = ESCAPE_IDENTIFIER(next_token);
        appendStringInfo(buf, "NOT EXISTS (SELECT 1 FROM %s WHERE %s IS NOT NULL)", escaped_table, escaped_field);
        pfree(escaped_table);
    } 
    else if (!strcmp(operator, "_is_true")) 
    {
        appendStringInfo(buf, "%s IS TRUE", escaped_field);
    } 
    else if (!strcmp(operator, "_is_false")) 
    {
        appendStringInfo(buf, "%s IS FALSE", escaped_field);
    }
    else 
    {
        pg_hier_error_unknown_operator(operator);
    }
    
    next_token = GET_TOKEN(saveptr);
    if (strcmp(next_token, "}"))
        pg_hier_error_expected_token("}", next_token, "condition");

    (*lvl)--;
    pfree(escaped_field);
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
        text *parent_text;
        text *child_text;
        Oid argtypes[3];
        Datum values[3];
        int last_idx;
        
        if ((ret = SPI_connect()) != SPI_OK_CONNECT)
            pg_hier_error_spi_connect(ret);

        parent_text = parent ? cstring_to_text(parent) : NULL;
        child_text = child ? cstring_to_text(child) : NULL;

        argtypes[0] = INT4OID;
        argtypes[1] = TEXTOID;
        argtypes[2] = TEXTOID;
        values[0] = Int32GetDatum(hh->hier_id);
        values[1] = PointerGetDatum(parent_text);
        values[2] = PointerGetDatum(child_text);

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
            last_idx = SPI_processed - 1;

            appendStringInfoString(buf, child);

            for (int i = 0; i < last_idx; i++)
            {
                HeapTuple tuple;
                bool isnull_parent, isnull_child, isnull_parent_key, isnull_child_key;
                Datum parent_name_datum, parent_key_datum, child_name_datum, child_key_datum;
                char *parent_name;
                char *child_name;
                ArrayType *parent_key_array, *child_key_array;
                Datum *parent_key_elems, *child_key_elems;
                bool *parent_key_nulls, *child_key_nulls;
                int parent_key_nelems, child_key_nelems;
                int16 typlen;
                bool typbyval;
                char typalign;
                char *parent_key;

                tuple = SPI_tuptable->vals[i];

                parent_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull_parent);
                parent_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull_parent_key);
                child_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 3, &isnull_child);
                child_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 4, &isnull_child_key);

                if (isnull_parent || isnull_child || isnull_parent_key || isnull_child_key)
                {
                    pg_hier_warning_null_values(i);
                    continue;
                }

                parent_name = TextDatumGetCString(parent_name_datum);
                child_name = TextDatumGetCString(child_name_datum);

                // Process array keys
                parent_key_array = DatumGetArrayTypeP(parent_key_datum);
                child_key_array = DatumGetArrayTypeP(child_key_datum);

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
                    char *child_key;
                    
                    if (j > 0)
                        appendStringInfoString(buf, " AND ");

                    parent_key = TextDatumGetCString(parent_key_elems[j]);
                    child_key = TextDatumGetCString(child_key_elems[j]);

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
                HeapTuple tuple;
                bool isnull_parent, isnull_child, isnull_parent_key, isnull_child_key;
                Datum parent_name_datum, parent_key_datum, child_name_datum, child_key_datum;
                char *parent_name;
                char *child_name;
                ArrayType *parent_key_array, *child_key_array;
                Datum *parent_key_elems, *child_key_elems;
                bool *parent_key_nulls, *child_key_nulls;
                int parent_key_nelems, child_key_nelems;
                int16 typlen;
                bool typbyval;
                char typalign;
                char *parent_key;
                
                tuple = SPI_tuptable->vals[last_idx];

                parent_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, &isnull_parent);
                parent_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 2, &isnull_parent_key);
                child_name_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 3, &isnull_child);
                child_key_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 4, &isnull_child_key);

                if (!isnull_parent && !isnull_child && !isnull_parent_key && !isnull_child_key)
                {
                    parent_name = TextDatumGetCString(parent_name_datum);
                    child_name = TextDatumGetCString(child_name_datum);

                    // Process array keys
                    parent_key_array = DatumGetArrayTypeP(parent_key_datum);
                    child_key_array = DatumGetArrayTypeP(child_key_datum);

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
                        char *child_key;
                        
                        if (j > 0)
                            appendStringInfoString(buf, " AND ");

                        parent_key = TextDatumGetCString(parent_key_elems[j]);
                        child_key = TextDatumGetCString(child_key_elems[j]);

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
    bool isnull;
    Datum val;

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
            val = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull);

            if (!isnull)
                result = datumCopy(val, false, -1);
        }
    }

    SPI_finish();

    return result;
}
