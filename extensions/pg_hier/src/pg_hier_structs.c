#include "pg_hier_structs.h"

/**************************************
 * String array functions
 **************************************/
struct string_array *
create_string_array(void)
{
    struct string_array *arr = palloc(sizeof(struct string_array));
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
    return arr;
}

void add_string_to_array(struct string_array *arr, char *value)
{
    if ((arr->size + 1) >= arr->capacity)
    {
        arr->capacity = arr->capacity > 0 ? arr->capacity * 2 : 4;
        arr->data = arr->data ? repalloc(arr->data, arr->capacity * sizeof(char *)) : palloc(arr->capacity * sizeof(char *));
    }
    arr->data[arr->size++] = pstrdup(value);
}

void free_string_array(struct string_array *arr)
{
    if (arr != NULL)
    {
        if (arr->data)
        {
            for (int i = 0; i < arr->size; ++i)
            {
                pfree(arr->data[i]);
            }
            pfree(arr->data);
            arr->data = NULL;
        }
        pfree(arr);
    }
}

/**************************************
 * Table stack functions
 **************************************/
struct table_stack *
create_table_stack_entry(char *table_name, struct table_stack *next)
{
    struct table_stack *new_entry = palloc(sizeof(struct table_stack));
    new_entry->table_name = pstrdup(table_name);
    new_entry->next = next;
    return new_entry;
}

void pop_table_stack(struct table_stack **stack)
{
    if (stack == NULL || *stack == NULL)
    {
        return;
    }
    struct table_stack *top = *stack;
    *stack = top->next;
    pfree(top->table_name);
    pfree(top);
}

void free_table_stack(struct table_stack **stack)
{
    while (*stack)
    {
        struct table_stack *next = (*stack)->next;
        pfree((*stack)->table_name);
        pfree(*stack);
        *stack = next;
    }
}