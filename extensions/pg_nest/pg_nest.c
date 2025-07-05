#include "postgres.h"
#include "fmgr.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/array.h"
#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "catalog/namespace.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_nest_many);

Datum
pg_nest_many(PG_FUNCTION_ARGS)
{
    //elog(INFO, "START: pg_nest_many");

    text *query_text = PG_GETARG_TEXT_PP(0);
    text *fk_text = PG_GETARG_TEXT_PP(1);
    int32 parent_id = PG_GETARG_INT32(2);
    text *type_name_text = PG_GETARG_TEXT_PP(3);
    if (!PG_ARGISNULL(4)) {
        (void) PG_GETARG_HEAPTUPLEHEADER(4);  // Safe now
    }


    char *query = text_to_cstring(query_text);
    char *foreign_key = text_to_cstring(fk_text);
    char *type_name = text_to_cstring(type_name_text);

    //elog(INFO, "Query: %s", query);
    //elog(INFO, "Foreign Key: %s", foreign_key);
    //elog(INFO, "Parent ID: %d", parent_id);
    //elog(INFO, "Type Name: %s", type_name);

    char *dot = strchr(foreign_key, '.');
    char *column = (dot != NULL) ? dot + 1 : foreign_key;

    int sql_len = strlen(query) + strlen(column) + 64;
    char *modified_sql = palloc(sql_len);
    snprintf(modified_sql, sql_len, "%s WHERE %s = %d", query, column, parent_id);

    //elog(INFO, "Modified SQL: %s", modified_sql);

    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed");

    if (SPI_execute(modified_sql, true, 0) != SPI_OK_SELECT)
        elog(ERROR, "SPI_execute failed");

    SPITupleTable *tuptable = SPI_tuptable;
    uint64 proc = SPI_processed;

    elog(INFO, "SPI returned %lu rows", proc);

    Oid elem_type = TypenameGetTypid(type_name);
    if (!OidIsValid(elem_type)) {
        elog(ERROR, "Could not resolve typename: %s", type_name);
    }

    TupleDesc tupdesc = lookup_rowtype_tupdesc(elem_type, -1);

    //elog(INFO, "Composite type OID: %u", elem_type);
    //elog(INFO, "Creating arraybuildstate...");

    ArrayBuildState *astate = NULL;

    for (uint64 i = 0; i < proc; i++)
    {
        //elog(INFO, "Processing row %lu", i + 1);

        HeapTuple tuple = tuptable->vals[i];
        int natts = tupdesc->natts;

        Datum *values = (Datum *) palloc(natts * sizeof(Datum));
        bool *nulls = (bool *) palloc(natts * sizeof(bool));

        for (int j = 0; j < natts; j++)
        {
            bool isnull;
            values[j] = SPI_getbinval(tuple, tupdesc, j + 1, &isnull);
            nulls[j] = isnull;
        }

        //elog(INFO, "Values and nulls extracted");

        HeapTuple new_tuple = heap_form_tuple(tupdesc, values, nulls);
        Datum composite = HeapTupleGetDatum(new_tuple);

        //elog(INFO, "Tuple formed. Adding to array");

        astate = accumArrayResult(astate, composite, false, elem_type, CurrentMemoryContext);

        //elog(INFO, "Tuple added to array");
    }

    Datum result;
    if (astate == NULL)
    {
        //elog(INFO, "Returning: EMPTY array");
        result = PointerGetDatum(construct_empty_array(elem_type));
    }
    else
    {
        //elog(INFO, "Returning: FILLED array");
        result = makeArrayResult(astate, CurrentMemoryContext);
    }

    DecrTupleDescRefCount(tupdesc);

    SPI_finish();
    PG_RETURN_DATUM(result);
}
