#ifndef PG_HIER_STRUCTS_H
#define PG_HIER_STRUCTS_H

#include "pg_hier_dependencies.h"

typedef struct string_array
{
    char **data;
    int size;
    int capacity;
} string_array;

typedef struct table_stack
{
    char *table_name;
    bool first_column;
    StringInfoData where_condition;
    struct table_stack *next;
} table_stack;

typedef struct hier_header
{
    char hier[1024];
    int hier_id;
    int deepest_nest; // table->size - 1 on hier update
} hier_header;

typedef struct table_position
{
    int original_position;
    int hierarchy_position;
} table_position;

typedef struct ColumnArrayState
{
    int allocated;
    int num_columns;
    text **column_names;
    List **column_values;
    List **column_nulls;
    Oid *column_types;
} ColumnArrayState;

typedef struct
{
    int num_keys;
    bool keys_initialized;
    text **keys;
} KeyStore;

string_array *create_string_array(void);
void add_string_to_array(string_array *arr, char *value);
void copy_string_array(string_array *to, string_array *from);
void free_string_array(string_array *arr);

// No need for a init, table_stack *next is NULL on first call
table_stack *create_table_stack_entry(char *table_name, table_stack *next);
char *peek_table_stack(table_stack **stack);
char *pop_table_stack(table_stack **stack);
void free_table_stack(table_stack **stack);

hier_header *create_hier_header0(void);
hier_header *create_hier_header2(char *hier, int hier_id);
void update_hier_header(hier_header *hh, char *hier, int hier_id);
void free_hier_header(hier_header *hh);

// Macros for hier_header creation
#define CREATE_HIER_HEADER(...) \
    _GET_HIER_HEADER_MACRO(__VA_ARGS__, create_hier_header2, create_hier_header0)(__VA_ARGS__)
#define _GET_HIER_HEADER_MACRO(_1, _2, NAME, ...) NAME

#endif /* PG_HIER_STRUCTS_H */
