#ifndef PG_HIER_H
#define PG_HIER_H

#include "pg_hier_dependencies.h"

void parse_input(StringInfo buf, const char *input, 
                        struct table_stack **stack, struct string_array **tables);
char *trim_whitespace(char *str);
int pg_hier_find_hier(struct string_array *name_path_elems);

#endif /* PG_HIER_H */

void
parse_input(StringInfo buf, const char *input, struct table_stack **stack, struct string_array **tables)
{
    char *input_copy = pstrdup(input);
    char *token;
    char *next_token = NULL;
    char *prev_token = NULL;
    char *saveptr;
    bool first = true;

    token = strtok_r(input_copy, ", \n\r\t", &saveptr);

    struct table_stack *new_entry = palloc(sizeof(struct table_stack));
    new_entry->table_name = pstrdup(token);
    new_entry->next = *stack;
    *stack = new_entry;
    token = strtok_r(NULL, ", \n\r\t", &saveptr);

    (*tables)->data = palloc(sizeof(char *) * 4);
    (*tables)->data[0] = pstrdup(token);
    (*tables)->size = 1;
    (*tables)->capacity = 4;

    if (*token != '{')
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
            struct table_stack *top = *stack;
            *stack = (*stack)->next;
            pfree(top->table_name);
            pfree(top);
            token = strtok_r(NULL, ", \n\r\t", &saveptr);
            continue;
        }

        next_token = strtok_r(NULL, ", \n\r\t", &saveptr);

        if (next_token && *next_token == '{')
        {
            struct table_stack *new_entry = palloc(sizeof(struct table_stack));
            new_entry->table_name = pstrdup(token);
            new_entry->next = *stack;
            *stack = new_entry;
            token = strtok_r(NULL, ", \n\r\t", &saveptr);

            (*tables)->size++;
            if ((*tables)->size >= (*tables)->capacity)
            {
                (*tables)->capacity = (*tables)->capacity ? (*tables)->capacity * 2 : 4;
                (*tables)->data = repalloc((*tables)->data, (*tables)->capacity * sizeof(char *));
            }
            (*tables)->data[(*tables)->size - 1] = pstrdup(token);
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

char *
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

int 
pg_hier_find_hier(struct string_array *name_path_elems)
{
    if (name_path_elems == NULL || name_path_elems->size < 2)
    {
        ereport(ERROR, (errmsg("Name path elements must contain at least two elements")));
    }

    StringInfoData query;
    initStringInfo(&query);
    appendStringInfo(&query, "SELECT id FROM pg_hier_header WHERE ");

    for (int i = 0; i < name_path_elems->size; i++)
    {
        if (i > 0) appendStringInfoString(&query, " AND ");
        appendStringInfo(&query, "table_path LIKE '%%%s%%'", name_path_elems->data[i]);
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
    if (SPI_processed > 0)
    {
        HeapTuple tuple = SPI_tuptable->vals[0];
        Datum id_datum = SPI_getbinval(tuple, SPI_tuptable->tupdesc, 1, NULL);
        hier_id = DatumGetUInt64(id_datum);
    }

    SPI_finish();
    
    return hier_id;
}
