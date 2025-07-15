#include "pg_hier_structs.h"

/**************************************
 * String array functions
 **************************************/
string_array *
create_string_array(void)
{
    string_array *arr = palloc(sizeof(string_array));
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
    return arr;
}

void add_string_to_array(string_array *arr, char *value)
{
    if ((arr->size + 1) >= arr->capacity)
    {
        arr->capacity = arr->capacity > 0 ? arr->capacity * 2 : 4;
        arr->data = arr->data ? repalloc(arr->data, arr->capacity * sizeof(char *)) : palloc(arr->capacity * sizeof(char *));
    }
    arr->data[arr->size++] = pstrdup(value);
}

void copy_string_array(string_array *to, string_array *from)
{
    to->data = palloc(from->size * sizeof(char *));
    to->size = from->size;
    to->capacity = from->capacity;

    for (int i = 0; i < from->size; i++)
        to->data[i] = pstrdup(from->data[i]);
}

void free_string_array(string_array *arr)
{
    if (arr != NULL)
    {
        if (arr->data)
        {
            for (int i = 0; i < arr->size; ++i)
                pfree(arr->data[i]);
            pfree(arr->data);
            arr->data = NULL;
        }
        pfree(arr);
    }
    arr = NULL;
}

/**************************************
 * Table stack functions
 **************************************/
table_stack *
create_table_stack_entry(char *table_name, table_stack *next)
{
    table_stack *new_entry = palloc(sizeof(table_stack));
    new_entry->table_name = pstrdup(table_name);
    new_entry->first_column = true;
    initStringInfo(&new_entry->where_condition);
    new_entry->next = next;
    return new_entry;
}

char *
peek_table_stack(table_stack **stack)
{
    return (stack && *stack) ? pstrdup((*stack)->table_name) : NULL;
}

table_stack *
pop_table_stack(table_stack **stack)
{
    table_stack *top = NULL;
    if (stack && *stack)
    {
        top = *stack;
        *stack = top->next;
        top->next = NULL;
    }
    return top;
}

void 
free_table_stack(table_stack **stack)
{
    while (*stack)
    {
        table_stack *next = (*stack)->next;
        pfree((*stack)->table_name);
        if ((*stack)->where_condition.data)
            pfree((*stack)->where_condition.data);
        pfree(*stack);
        *stack = next;
    }
}

/**************************************
 * Hier header functions
 **************************************/
hier_header *
create_hier_header0(void)
{
    hier_header *hh = palloc(sizeof(hier_header));
    strcpy(hh->hier, "\0");
    hh->hier_id = -1;
    hh->deepest_nest = -1;
    return hh;
}

hier_header *
create_hier_header2(char *hier, int hier_id)
{
    hier_header *hh = palloc(sizeof(hier_header));
    strcpy(hh->hier, hier);
    hh->hier_id = hier_id;
    hh->deepest_nest = -1;
    return hh;
}

void update_hier_header(hier_header *hh, char *hier, int hier_id)
{
    strcpy(hh->hier, hier);
    hh->hier_id = hier_id;
}

void free_hier_header(hier_header *hh)
{
    if (hh)
        free(hh);
    hh = NULL;
}