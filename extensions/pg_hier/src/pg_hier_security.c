#include "pg_hier_security.h"
#include "pg_hier_errors.h"
#include <ctype.h>
#include <string.h>

/**************************************
 * Security validation functions
 **************************************/

bool 
pg_hier_is_valid_identifier(const char *name)
{
    if (!name || *name == '\0')
        return false;
    
    // Must start with letter or underscore
    if (!isalpha((unsigned char)*name) && *name != '_')
        return false;
    
    // Check rest of identifier
    for (const char *p = name + 1; *p; p++)
    {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '$')
            return false;
    }
    
    // Check length (PostgreSQL limit is 63 characters)
    if (strlen(name) > 63)
        return false;
    
    return true;
}

char *
pg_hier_escape_identifier(const char *name)
{
    text *input_text;
    Datum quoted_datum;
    char *result;
    
    if (!name)
        return NULL;
    
    // Use PostgreSQL's quote_ident function for proper escaping
    input_text = cstring_to_text(name);
    quoted_datum = DirectFunctionCall1(quote_ident, PointerGetDatum(input_text));
    result = TextDatumGetCString(quoted_datum);
    
    pfree(input_text);
    return result;
}

char *
pg_hier_escape_literal(const char *value)
{
    text *input_text;
    Datum quoted_datum;
    char *result;
    
    if (!value)
        return pstrdup("NULL");
    
    // Use PostgreSQL's quote_literal function for proper escaping
    input_text = cstring_to_text(value);
    quoted_datum = DirectFunctionCall1(quote_literal, PointerGetDatum(input_text));
    result = TextDatumGetCString(quoted_datum);
    
    pfree(input_text);
    return result;
}

bool 
pg_hier_validate_table_name(const char *table_name)
{
    char *dot;
    bool schema_valid;
    
    if (!table_name || *table_name == '\0')
        return false;
    
    // Check for schema.table format
    dot = strchr(table_name, '.');
    if (dot)
    {
        // Validate schema part
        *dot = '\0';
        schema_valid = pg_hier_is_valid_identifier(table_name);
        *dot = '.';
        
        if (!schema_valid)
            return false;
        
        // Validate table part
        return pg_hier_is_valid_identifier(dot + 1);
    }
    
    return pg_hier_is_valid_identifier(table_name);
}

bool 
pg_hier_validate_column_name(const char *column_name)
{
    return pg_hier_is_valid_identifier(column_name);
}

char *
pg_hier_sanitize_like_pattern(const char *pattern)
{
    StringInfoData escaped;
    if (!pattern)
        return NULL;

    initStringInfo(&escaped);
    
    appendStringInfoChar(&escaped, '\'');
    appendStringInfoChar(&escaped, '%');
    
    for (const char *p = pattern; *p; p++)
    {
        switch (*p)
        {
            case '\'':
                appendStringInfoString(&escaped, "''");
                break;
            case '%':
                appendStringInfoString(&escaped, "\\%");
                break;
            case '_':
                appendStringInfoString(&escaped, "\\_");
                break;
            case '\\':
                appendStringInfoString(&escaped, "\\\\");
                break;
            default:
                appendStringInfoChar(&escaped, *p);
                break;
        }
    }
    
    appendStringInfoChar(&escaped, '%');
    appendStringInfoChar(&escaped, '\'');
    
    return escaped.data;
}
