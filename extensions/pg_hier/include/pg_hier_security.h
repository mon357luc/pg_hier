#ifndef PG_HIER_SECURITY_H
#define PG_HIER_SECURITY_H

#include "pg_hier_dependencies.h"

/* Security validation functions */
void validate_input_security(const char *input);
bool pg_hier_is_valid_identifier(const char *name);
char *pg_hier_escape_identifier(const char *name);
char *pg_hier_escape_literal(const char *value);
bool pg_hier_validate_table_name(const char *table_name);
bool pg_hier_validate_column_name(const char *column_name);
char *pg_hier_sanitize_like_pattern(const char *pattern);

/* SQL injection prevention macros */
#define VALIDATE_IDENTIFIER(name, context) \
    do { \
        if (!pg_hier_is_valid_identifier(name)) { \
            PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT, \
                "Invalid identifier '%s' in %s", name, context); \
        } \
    } while(0)

#define ESCAPE_IDENTIFIER(name) pg_hier_escape_identifier(name)
#define ESCAPE_LITERAL(value) pg_hier_escape_literal(value)

#endif /* PG_HIER_SECURITY_H */
