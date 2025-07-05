#ifndef PG_HIER_HELPER_H
#define PG_HIER_HELPER_H

#include "pg_hier_dependencies.h"
#include "pg_hier_structs.h"

void parse_input(StringInfo buf, const char *input, struct table_stack **stack, struct string_array **tables);
char *trim_whitespace(char *str);
int pg_hier_find_hier(struct string_array *tables);
void reorder_tables(struct string_array *tables, char *hierarchy_string);

#endif /* PG_HIER_HELPER_H */