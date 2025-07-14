#ifndef PG_HIER_HELPER_H
#define PG_HIER_HELPER_H

#include "pg_hier_dependencies.h"
#include "pg_hier_structs.h"
#include "pg_hier_sql.h"
#include "pg_hier_errors.h"

void parse_input(StringInfo buf, const char *input, string_array **tables);

/* Forward declarations for static functions used in macros */
char *first_token(const char *input, char **saveptr);
char *get_next_token(char **saveptr);
bool is_special_char(char c);
char *trim_whitespace(char *str);
void _and_operator(StringInfo buf, char **saveptr, int *lvl);
void _or_operator(StringInfo buf, char **saveptr, int *lvl);
void _parse_condition(StringInfo buf, char *field_name, char **saveptr, int *lvl);

void pg_hier_from_clause(StringInfo buf, hier_header *hh, char *parent, char *child);
void pg_hier_get_hier(string_array *tables, hier_header *hh);
void pg_hier_find_hier(string_array *tables, hier_header *hh);

void pg_hier_where_clause(StringInfo buf, char *next_token, char **saveptr);

Datum pg_hier_return_one(const char *sql);

#define GET_TOKEN_1(saveptr) trim_whitespace(get_next_token(saveptr))
#define GET_TOKEN_2(inp, saveptr) trim_whitespace(first_token(inp, saveptr))
#define GET_TOKEN_SELECT(_1, _2, NAME, ...) NAME
#define GET_TOKEN(...) GET_TOKEN_SELECT(__VA_ARGS__, GET_TOKEN_2, GET_TOKEN_1)(__VA_ARGS__)

#endif /* PG_HIER_HELPER_H */