#ifndef PG_HIER_DEPENDENCIES_H
#define PG_HIER_DEPENDENCIES_H

#include "postgres.h"            // Basic PostgreSQL definitions
#include "fmgr.h"                // Function manager definitions
#include <utils/array.h>         // For array manipulation
#include <utils/syscache.h>      // System cache access
#include <utils/lsyscache.h>     // System cache access
#include "utils/json.h"          // For json datatype and functions
#include "utils/jsonb.h"         // For jsonb datatype and functions
#include <catalog/pg_type.h>     // PostgreSQL type definitions
#include <utils/builtins.h>      // Built-in data types (text, etc.)
#include <access/htup_details.h> // Tuple details (HeapTuple)
#include <utils/typcache.h>      // Type cache utilities
#include <funcapi.h>             // Function API
#include <executor/spi.h>        // Server Programming Interface
#include <executor/executor.h>   // Executor definitions
#include <utils/datum.h>         //Datum SPIs

#endif /* PG_HIER_DEPENDENCIES_H */