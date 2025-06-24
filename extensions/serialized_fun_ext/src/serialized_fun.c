#include "postgres.h"
#include "fmgr.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "executor/executor.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* ---------- helper: turn type‐OID into readable type name */
static inline char *
type_name(Oid typid)
{
    return format_type_be(typid);   /* e.g. 23  -> "int4", 25 -> "text" */
}

/* ===================================================================
 *  1)  header  (non-aggregate)
 *      SELECT hier_header(int4, text, timestamptz);
 * ===================================================================*/
PG_FUNCTION_INFO_V1(compile_check);

Datum compile_check(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(cstring_to_text("Check 1"));
}

PG_FUNCTION_INFO_V1(hier_header);

Datum
hier_header(PG_FUNCTION_ARGS)
{
    StringInfoData buf;
    initStringInfo(&buf);
    appendStringInfoString(&buf, "{ header: { ");

    for (int i = 0; i < PG_NARGS(); i++)
    {
        if (i) appendStringInfoString(&buf, ", ");
        appendStringInfo(&buf,
                         "field%d: %s",
                         i + 1,
                         type_name(get_fn_expr_argtype(fcinfo->flinfo, i)));
    }
    appendStringInfoString(&buf, " } }");

    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

/* ===================================================================
 *  2)  rows  (aggregate)
 * ===================================================================*/
typedef struct
{
    StringInfoData buf;
    uint64  rowno;
} HierState;

PG_FUNCTION_INFO_V1(hierify_sfunc);
Datum
hierify_sfunc(PG_FUNCTION_ARGS)
{
    MemoryContext aggctx = NULL;
#if PG_VERSION_NUM >= 90600
    if (!AggCheckCallContext(fcinfo, &aggctx))
        elog(ERROR, "hierify_sfunc called in non-aggregate context");
#else
    aggctx = ((AggState *) fcinfo->context)->ss.ps.state->es_query_cxt;
    if (!aggctx)
        elog(ERROR, "hierify_sfunc called in non-aggregate context");
#endif

    HierState *state;

    /* first call? ----------------------------------------------------*/
    if (PG_ARGISNULL(0))
    {
        MemoryContext old = MemoryContextSwitchTo(aggctx);
        state = (HierState *) palloc0(sizeof(HierState));
        initStringInfo(&state->buf);
        appendStringInfoChar(&state->buf, '{');
        state->rowno = 0;
        MemoryContextSwitchTo(old);
    }
    else
        state = (HierState *) PG_GETARG_POINTER(0);

    /* ----------------------------------------------------------------
     *  append one row
     * ----------------------------------------------------------------*/
    state->rowno++;
    if (state->rowno > 1)
        appendStringInfoChar(&state->buf, '\n');  /* new line for next row */

    appendStringInfo(&state->buf, "row%llu: { ",
                     (unsigned long long) state->rowno);

    for (int i = 1; i < PG_NARGS(); i++)
    {
        if (i > 1) appendStringInfoString(&state->buf, ", ");

        if (PG_ARGISNULL(i))
        {
            appendStringInfo(&state->buf,
                             "\"field%d\": NULL", i);
        }
        else
        {
            Oid  typid      = get_fn_expr_argtype(fcinfo->flinfo, i);
            Oid  outFuncOid = InvalidOid;
            bool isVarlen;
            getTypeOutputInfo(typid, &outFuncOid, &isVarlen);

            char *val = OidOutputFunctionCall(outFuncOid,
                                              PG_GETARG_DATUM(i));
            appendStringInfo(&state->buf,
                             "\"field%d\": \"%s\"", i, val);
        }
    }
    appendStringInfoString(&state->buf, " }");

    PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(hierify_ffunc);
Datum
hierify_ffunc(PG_FUNCTION_ARGS)
{
    HierState *state = (HierState *) PG_GETARG_POINTER(0);

    appendStringInfoChar(&state->buf, '}');  /* close outer brace */
    text *out = cstring_to_text(state->buf.data);

    pfree(state->buf.data);
    pfree(state);

    PG_RETURN_TEXT_P(out);
}