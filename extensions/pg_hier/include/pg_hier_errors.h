#ifndef PG_HIER_ERRORS_H
#define PG_HIER_ERRORS_H

#include "postgres.h"
#include "utils/elog.h"

/*
 * Custom Error Codes for pg_hier Extension
 * Using custom SQLSTATE codes in the PH (PostgreSQL Hierarchical) range
 */
#define ERRCODE_PG_HIER_INVALID_INPUT         MAKE_SQLSTATE('P','H','0','0','1')
#define ERRCODE_PG_HIER_SYNTAX_ERROR          MAKE_SQLSTATE('P','H','0','0','2')
#define ERRCODE_PG_HIER_MISSING_HIERARCHY     MAKE_SQLSTATE('P','H','0','0','3')
#define ERRCODE_PG_HIER_INVALID_OPERATOR      MAKE_SQLSTATE('P','H','0','0','4')
#define ERRCODE_PG_HIER_INSUFFICIENT_TABLES   MAKE_SQLSTATE('P','H','0','0','5')
#define ERRCODE_PG_HIER_DATABASE_ERROR        MAKE_SQLSTATE('P','H','0','0','6')
#define ERRCODE_PG_HIER_MEMORY_ERROR          MAKE_SQLSTATE('P','H','0','0','7')
#define ERRCODE_PG_HIER_INVALID_CONDITION     MAKE_SQLSTATE('P','H','0','0','8')

/*
 * Error Reporting Macros
 */
#define PG_HIER_ERROR(code, msg, ...) \
    ereport(ERROR, \
        (errcode(code), \
         errmsg(msg, ##__VA_ARGS__)))

#define PG_HIER_WARNING(msg, ...) \
    ereport(WARNING, \
        (errmsg(msg, ##__VA_ARGS__)))

#define PG_HIER_INFO(msg, ...) \
    ereport(INFO, \
        (errmsg(msg, ##__VA_ARGS__)))

#define PG_HIER_DEBUG(msg, ...) \
    ereport(DEBUG1, \
        (errmsg(msg, ##__VA_ARGS__)))

/*
 * Specific Error Functions with Context
 */

/* Input validation errors */
#define pg_hier_error_empty_input() \
    PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT, \
        "Empty input string or no valid table name found")

#define pg_hier_error_no_tables() \
    PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT, \
        "No tables found in input string")

#define pg_hier_error_insufficient_tables() \
    PG_HIER_ERROR(ERRCODE_PG_HIER_INSUFFICIENT_TABLES, \
        "At least two tables are needed for hierarchical query")

#define pg_hier_error_insufficient_path_elements() \
    PG_HIER_ERROR(ERRCODE_PG_HIER_INSUFFICIENT_TABLES, \
        "Name path elements must contain at least two elements")

/* Syntax errors */
#define pg_hier_error_expected_token(expected, got, context) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_SYNTAX_ERROR, \
        "Expected '%s' but got '%s' while parsing %s", \
        expected, got ? got : "NULL", context)

#define pg_hier_error_expected_where(got) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_SYNTAX_ERROR, \
        "Expected 'where' but got '%s'", got)

#define pg_hier_error_unmatched_braces() \
    PG_HIER_ERROR(ERRCODE_PG_HIER_SYNTAX_ERROR, \
        "Unmatched closing brace in input")

#define pg_hier_error_mismatched_braces() \
    PG_HIER_ERROR(ERRCODE_PG_HIER_SYNTAX_ERROR, \
        "Mismatched braces in where clause")

#define pg_hier_error_unexpected_end(after) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_SYNTAX_ERROR, \
        "Unexpected end of input after %s '%s'", \
        "operator", after)

/* Operator and condition errors */
#define pg_hier_error_unknown_operator(op) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_OPERATOR, \
        "Unknown operator '%s'", op)

#define pg_hier_error_invalid_between_values(lower, upper) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_CONDITION, \
        "Expected two values for '_between', got '%s' and '%s'", \
        lower ? lower : "NULL", upper ? upper : "NULL")

#define pg_hier_error_invalid_key_format(key) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_SYNTAX_ERROR, \
        "Invalid key format: %s", key)

/* Hierarchy lookup errors */
#define pg_hier_error_no_hierarchy_data(parent, child) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_MISSING_HIERARCHY, \
        "No hierarchy data found for parent=%s, child=%s", \
        parent, child)

#define pg_hier_error_no_path_found(parent, child) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_MISSING_HIERARCHY, \
        "No path found from %s to %s", parent, child)

/* Database operation errors */
#define pg_hier_error_spi_connect(ret_code) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_DATABASE_ERROR, \
        "SPI_connect failed: %s", SPI_result_code_string(ret_code))

#define pg_hier_error_spi_execute(ret_code) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_DATABASE_ERROR, \
        "SPI_execute failed: %s", SPI_result_code_string(ret_code))

#define pg_hier_error_spi_execute_with_args(ret_code) \
    PG_HIER_ERROR(ERRCODE_PG_HIER_DATABASE_ERROR, \
        "SPI_execute_with_args failed: %d", ret_code)

/* Warning messages */
#define pg_hier_warning_missing_params() \
    PG_HIER_WARNING("Missing required parameters for hierarchy lookup")

#define pg_hier_warning_null_values(row_num) \
    PG_HIER_WARNING("Row %d has NULL values in critical fields", row_num)

/* Info messages */
#define pg_hier_info_where_clause(action, table) \
    PG_HIER_INFO("%s where clause: %s", action, table)

#define pg_hier_info_subquery(action, table) \
    PG_HIER_INFO("%s subquery: %s", action, table)

#define pg_hier_info_adding_where(clause) \
    PG_HIER_INFO("Adding where clause: WHERE %s", clause)

#define pg_hier_info_table_check(table) \
    PG_HIER_INFO("Current table check: %s", table)

/*
 * Helper macros for common error patterns
 */
#define VALIDATE_NOT_NULL(ptr, name) \
    do { \
        if (!(ptr)) { \
            PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT, \
                "Required parameter '%s' is NULL", name); \
        } \
    } while(0)

#define VALIDATE_STRING_NOT_EMPTY(str, name) \
    do { \
        if (!(str) || *(str) == '\0') { \
            PG_HIER_ERROR(ERRCODE_PG_HIER_INVALID_INPUT, \
                "Required string parameter '%s' is empty", name); \
        } \
    } while(0)

#define CHECK_SPI_RESULT(ret, expected) \
    do { \
        if ((ret) != (expected)) { \
            pg_hier_error_spi_execute(ret); \
        } \
    } while(0)

#endif /* PG_HIER_ERRORS_H */
