#ifndef PG_HIER_HELPER_H
#define PG_HIER_HELPER_H

#include "pg_hier_dependencies.h"
#include "pg_hier_structs.h"

void parse_input(StringInfo buf, const char *input, struct string_array **tables);
char *get_token(char **saveptr);
char *trim_whitespace(char *str);
int pg_hier_find_hier(struct string_array *tables);
void reorder_tables(struct string_array *tables, char *hierarchy_string);
static int compare_string_positions(const void *a, const void *b);
JsonbValue datum_to_jsonb_value(Datum value_datum, Oid value_type);
Jsonb *get_or_create_array(Jsonb *existing_jsonb, char *key_str);
int find_column_index(ColumnArrayState *state, text *column_name);
void datum_to_jsonb(Datum val, Oid val_type, JsonbValue *result);

#endif /* PG_HIER_HELPER_H */