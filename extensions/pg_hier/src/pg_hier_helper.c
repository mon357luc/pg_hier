#include "pg_hier_helper.h"

static int compare_string_positions(const void *a, const void *b);

void
parse_input(StringInfo buf, const char *input, struct table_stack **stack, struct string_array **tables)
{
    char *input_copy = NULL;
    char *token = NULL;
    char *next_token = NULL;
    char *saveptr = NULL;
    bool first = true;

    PG_TRY();
    {
        input_copy = pstrdup(input); 
        *tables = create_string_array(); 

        token = strtok_r(input_copy, ", \n\r\t", &saveptr);

        if (token == NULL)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("Input string is empty or contains only delimiters."),
                    errdetail("Input was: %s", input_copy)));
        }

        *stack = create_table_stack_entry(token, *stack);  
        add_string_to_array(*tables, token);

        token = strtok_r(NULL, ", \n\r\t", &saveptr);

        if (token == NULL || strcmp(trim_whitespace(token), "{") != 0)
        {
            ereport(ERROR, (errmsg("Expected opening brace '{' after table name")));
        }

        token = strtok_r(NULL, ", \n\r\t", &saveptr);

        while (token != NULL)
        {
            token = trim_whitespace(token);
            if (*token == '\0')
            {
                token = strtok_r(NULL, ", \n\r\t", &saveptr);
                continue;
            }

            if (strcmp(token, "}") == 0)
            {
                if (*stack == NULL)
                {
                    ereport(ERROR, (errmsg("Unmatched closing brace in input")));
                }
                pop_table_stack(stack); 
                token = strtok_r(NULL, ", \n\r\t", &saveptr);
                continue;
            }

            next_token = strtok_r(NULL, ", \n\r\t", &saveptr);

            if (next_token && *next_token == '{')
            {
                *stack = create_table_stack_entry(token, *stack);
                add_string_to_array(*tables, token);
                token = strtok_r(NULL, ", \n\r\t", &saveptr);
                next_token = token;
                continue;
            }

            if (!first)
            {
                appendStringInfoString(buf, ", ");
            }
            first = false;

            if (*stack != NULL && (*stack)->table_name != NULL) 
            {
                appendStringInfo(buf, "%s.%s", (*stack)->table_name, token);
            }
            else
            {
                appendStringInfoString(buf, token);
            }

            token = next_token;
            next_token = NULL;
        }

        if (*stack != NULL)  
        {
            ereport(ERROR, (errmsg("Unmatched opening brace(s) in input.")));
        }
        if (input_copy != NULL)
        {
            pfree(input_copy);
        }
        input_copy = NULL;
    }
    PG_CATCH();
    {
        if (input_copy != NULL)
            pfree(input_copy);
        PG_RE_THROW(); 
    }
    PG_END_TRY();
}

/**************************************
 * Helper function to trim 
 * whitespace from a string
 **************************************/
char *
trim_whitespace(char *str)
{
    if (str == NULL) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) 
        return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int
pg_hier_find_hier(struct string_array *tables)
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
        if (i > 0) appendStringInfoString(&query, " AND ");
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
void
reorder_tables(struct string_array *tables, char *hierarchy_string)
{
    // Create an array to store the original positions of the tables
    int *original_positions = palloc(tables->size * sizeof(int));
    for (int i = 0; i < tables->size; i++)
    {
        original_positions[i] = i;
    }

    struct table_position {
        int original_position;
        int hierarchy_position;
    } *positions = palloc(tables->size * sizeof(struct table_position));

    for (int i = 0; i < tables->size; i++) {
        positions[i].original_position = i;
        char *table = tables->data[i];
        char *pos = strstr(hierarchy_string, table);
        if (pos) {
            positions[i].hierarchy_position = pos - hierarchy_string;
        } else {
             positions[i].hierarchy_position = strlen(hierarchy_string);
        }
    }

    qsort(positions, tables->size, sizeof(struct table_position), compare_string_positions);

    char **reordered_data = palloc(tables->size * sizeof(char*));

    for (int i = 0; i < tables->size; i++) {
         reordered_data[i] = pstrdup(tables->data[positions[i].original_position]); 
    }

    for(int i = 0; i < tables->size; ++i) {
         pfree(tables->data[i]);
    }
    pfree(tables->data);

    tables->data = reordered_data;

    pfree(positions);
    pfree(original_positions);
}

static int compare_string_positions(const void *a, const void *b) {
    struct table_position *posA = (struct table_position *)a;
    struct table_position *posB = (struct table_position *)b;

    return posA->hierarchy_position - posB->hierarchy_position;
}