#ifndef PG_HIER_STRUCTS_H
#define PG_HIER_STRUCTS_H

#include "pg_hier_dependencies.h"

struct string_array *create_string_array(void);
void add_string_to_array(struct string_array *arr, char *value);
void free_string_array(struct string_array *arr);

struct table_stack *create_table_stack_entry(char *table_name, struct table_stack *next);
void pop_table_stack(struct table_stack **stack);
void free_table_stack(struct table_stack *stack);

struct string_array {
    char **data;
    int size;
    int capacity;
};

struct table_stack {
    char *table_name;
    struct table_stack *next;
};

struct table_position {
    int original_position;
    int hierarchy_position;
};

#endif /* PG_HIER_STRUCTS_H */

