# Security Vulnerability Assessment and Fixes for pg_hier Extension

## Summary

The PostgreSQL extension `pg_hier` contained multiple critical SQL injection vulnerabilities that have been identified and fixed. This document outlines the vulnerabilities found and the security measures implemented.

## Vulnerabilities Identified

### 1. **Direct String Interpolation in WHERE Clauses** (Critical)
- **Location**: `pg_hier_helper.c` lines 510-570
- **Issue**: User input directly concatenated into SQL without escaping
- **Risk**: Complete database compromise through SQL injection

### 2. **Table Name Injection** (Critical)
- **Location**: Multiple locations in `pg_hier_helper.c`
- **Issue**: Table names not validated or escaped before use in SQL
- **Risk**: Access to unauthorized tables, schema enumeration

### 3. **Column Name Injection** (High)
- **Location**: Multiple locations in parsing logic
- **Issue**: Column names directly interpolated without validation
- **Risk**: Information disclosure, potential privilege escalation

### 4. **LIKE Pattern Injection** (High)
- **Location**: `pg_hier_find_hier` function
- **Issue**: LIKE patterns not escaped, allowing wildcard injection
- **Risk**: Information disclosure, performance degradation

### 5. **Arbitrary SQL Execution** (Critical)
- **Location**: `pg_hier_format` function
- **Issue**: Executes user-provided SQL without restrictions
- **Risk**: Complete database compromise

## Security Fixes Implemented

### 1. **Input Validation Layer**
- Created `pg_hier_security.h` and `pg_hier_security.c`
- Implemented identifier validation functions:
  - `pg_hier_is_valid_identifier()` - Validates SQL identifiers
  - `pg_hier_validate_table_name()` - Validates table names
  - `pg_hier_validate_column_name()` - Validates column names

### 2. **SQL Escaping Functions**
- `pg_hier_escape_identifier()` - Properly escapes SQL identifiers using PostgreSQL's `quote_ident`
- `pg_hier_escape_literal()` - Properly escapes SQL literals using PostgreSQL's `quote_literal`
- `pg_hier_sanitize_like_pattern()` - Escapes LIKE patterns

### 3. **Validation Macros**
- `VALIDATE_IDENTIFIER(name, context)` - Validates identifiers with context-aware error messages
- `ESCAPE_IDENTIFIER(name)` - Shorthand for identifier escaping
- `ESCAPE_LITERAL(value)` - Shorthand for literal escaping

### 4. **Function-Specific Fixes**

#### `_parse_condition()` Function
- Added validation for all field names
- All values now properly escaped using `ESCAPE_LITERAL`
- EXISTS/NOT EXISTS clauses validate table names
- All operators now use secure parameter interpolation

#### `parse_input()` Function
- Table names validated before use
- Column names validated and escaped
- All SQL generation uses escaped identifiers

#### `pg_hier_join()` Function
- Parent and child table names validated
- All table and column references properly escaped
- JOIN conditions use escaped identifiers

#### `pg_hier_format()` Function
- Restricted to SELECT statements only
- Blocks dangerous SQL keywords (DROP, DELETE, UPDATE, etc.)
- Prevents SQL comment injection
- Read-only transaction enforcement

#### `pg_hier_find_hier()` Function
- LIKE patterns properly sanitized
- Table names validated before pattern generation

### 5. **Additional Security Measures**
- Length validation for identifiers (63 character PostgreSQL limit)
- Character validation for identifiers (alphanumeric, underscore, dollar sign)
- Schema.table format support in validation
- Comprehensive error messages for debugging

## Testing

Created `test/sql/security_test.sql` to verify:
- Table name injection prevention
- Column name injection prevention  
- WHERE clause value escaping
- pg_hier_format restrictions
- Valid queries continue to work

## Usage Examples

### Before (Vulnerable)
```c
appendStringInfo(buf, "%s = %s", field_name, next_token);  // VULNERABLE
```

### After (Secure)
```c
VALIDATE_IDENTIFIER(field_name, "field name");
char *escaped_field = ESCAPE_IDENTIFIER(field_name);
char *escaped_value = ESCAPE_LITERAL(next_token);
appendStringInfo(buf, "%s = %s", escaped_field, escaped_value);  // SECURE
```

## Compatibility

These changes maintain backward compatibility for all legitimate use cases while preventing malicious input. The validation is strict but follows PostgreSQL identifier rules exactly.

## Recommendations

1. **Run Security Tests**: Execute the security test suite before deployment
2. **Code Review**: Review any future changes to SQL generation code
3. **Input Validation**: Always validate and escape user input in SQL contexts
4. **Principle of Least Privilege**: Ensure the extension runs with minimal required permissions
5. **Regular Audits**: Periodically review the codebase for new vulnerabilities

## Files Modified

- `include/pg_hier_security.h` (new)
- `src/pg_hier_security.c` (new)
- `include/pg_hier_helper.h` (updated)
- `src/pg_hier_helper.c` (updated)
- `src/pg_hier.c` (updated)
- `test/sql/security_test.sql` (new)

All security fixes follow PostgreSQL's built-in security functions and best practices for C extension development.
