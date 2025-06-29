#include "pg_hier.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pg_hier_format);

Datum
pg_hier_format(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    text *sql = PG_GETARG_TEXT_PP(0);
    char *query = text_to_cstring(sql);

    StringInfoData buf;
    initStringInfo(&buf);
    appendStringInfoChar(&buf, '{');

    int ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed: %d", ret);

    ret = SPI_execute(query, true, 0);
    if (ret != SPI_OK_SELECT)
    {
        SPI_finish();
        elog(ERROR, "SPI_execute failed: %d", ret);
    }

    // Add header metadata
    appendStringInfoString(&buf, "\"header\": { ");
    for (int col = 0; col < SPI_tuptable->tupdesc->natts; col++)
    {
        if (col > 0)
            appendStringInfoString(&buf, ", ");
        const char *colname = SPI_tuptable->tupdesc->attrs[col].attname.data;
        Oid typid = SPI_gettypeid(SPI_tuptable->tupdesc, col + 1);

        // Get type name
        char *typename = format_type_be(typid);

        appendStringInfo(&buf, "\"%s\": \"%s\"", colname, typename);
    }
    appendStringInfoString(&buf, " }, ");

    // Now output rows as before
    for (uint64 rowno = 0; rowno < SPI_processed; rowno++)
    {
        if (rowno > 0) appendStringInfoString(&buf, ",\n");

        appendStringInfo(&buf, "\"row%llu\": { ", (unsigned long long)(rowno + 1));

        for (int col = 0; col < SPI_tuptable->tupdesc->natts; col++)
        {
            if (col > 0)
                appendStringInfoString(&buf, ", ");

            bool isnull;
            Datum val = SPI_getbinval(SPI_tuptable->vals[rowno],
                                      SPI_tuptable->tupdesc, col + 1, &isnull);

            const char *colname = SPI_tuptable->tupdesc->attrs[col].attname.data;

            if (isnull)
            {
                appendStringInfo(&buf, "\"%s\": NULL", colname);
            }
            else
            {
                Oid typid = SPI_gettypeid(SPI_tuptable->tupdesc, col + 1);
                Oid outFuncOid;
                bool isVarlen;
                getTypeOutputInfo(typid, &outFuncOid, &isVarlen);

                char *outstr = OidOutputFunctionCall(outFuncOid, val);
                appendStringInfo(&buf, "\"%s\": \"%s\"", colname, outstr);
            }
        }
        appendStringInfoString(&buf, " }");
    }

    appendStringInfoChar(&buf, '}');
    SPI_finish();

    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}
