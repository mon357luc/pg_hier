#ifndef PG_HIER_HELPER_H
#define PG_HIER_HELPER_H

#include "pg_hier_dependencies.h"
#include "pg_hier_structs.h"
#include "pg_hier_sql.h"

void parse_input(StringInfo buf, const char *input, string_array **tables);
static char *first_token(const char *input, char **saveptr);
static char *get_next_token(char **saveptr);
static inline bool is_special_char(char c);
static char *trim_whitespace(char *str);

void pg_hier_from_clause(StringInfo buf, hier_header *hh, char *parent, char *child);
void pg_hier_get_hier(string_array *tables, hier_header *hh);
void pg_hier_find_hier(string_array *tables, hier_header *hh);
string_array *reorder_tables(string_array *tables, char *hierarchy_string);
static int compare_string_positions(const void *a, const void *b);

Datum pg_hier_return_one(const char *sql);

JsonbValue datum_to_jsonb_value(Datum value_datum, Oid value_type);
Jsonb *get_or_create_array(Jsonb *existing_jsonb, char *key_str);
int find_column_index(ColumnArrayState *state, text *column_name);
void datum_to_jsonb(Datum val, Oid val_type, JsonbValue *result);

#define GET_TOKEN_1(saveptr) trim_whitespace(get_next_token(saveptr))
#define GET_TOKEN_2(inp, saveptr) trim_whitespace(first_token(inp, saveptr))
#define GET_TOKEN_SELECT(_1, _2, NAME, ...) NAME
#define GET_TOKEN(...) GET_TOKEN_SELECT(__VA_ARGS__, GET_TOKEN_2, GET_TOKEN_1)(__VA_ARGS__)

#endif /* PG_HIER_HELPER_H */