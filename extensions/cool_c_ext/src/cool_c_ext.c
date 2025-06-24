#include "cool_c_ext.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum hello_world(PG_FUNCTION_ARGS);
Datum subq_csv(PG_FUNCTION_ARGS);
Datum csvify_sfunc(PG_FUNCTION_ARGS);
Datum csvify_ffunc(PG_FUNCTION_ARGS);

typedef struct CsvifyState
{
    StringInfoData buf;
} CsvifyState;

PG_FUNCTION_INFO_V1(hello_world);
// SELECT hello_world(ARRAY['Hello', NULL, 'World']);

Datum hello_world(PG_FUNCTION_ARGS)
{
    ArrayType *input = PG_GETARG_ARRAYTYPE_P(0);
    Datum *elements;
    bool *nulls;
    text *element;
    int num_elements;
    int i;
    StringInfoData result;

    initStringInfo(&result);

    deconstruct_array_builtin(input, TEXTOID, &elements, &nulls, &num_elements);

    for (i = 0; i < num_elements; i++)
    {
        if (i > 0)
        {
            appendStringInfoChar(&result, ',');
        }

        if (!nulls[i])
        {
            element = DatumGetTextP(elements[i]);
            appendStringInfoString(&result, text_to_cstring(element));
        }
        else
        {
            appendStringInfoString(&result, "[NULL]");
        }
    }

    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

PG_FUNCTION_INFO_V1(subq_csv);
// SELECT subq_csv(ROW(1, 'foo', NULL)::my_table);

Datum subq_csv(PG_FUNCTION_ARGS)
{
    HeapTupleHeader rec = PG_GETARG_HEAPTUPLEHEADER(0);
    Oid tupType = HeapTupleHeaderGetTypeId(rec);
    int32 tupTypmod = HeapTupleHeaderGetTypMod(rec);
    TupleDesc tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

    HeapTupleData tuple;
    tuple.t_len = HeapTupleHeaderGetDatumLength(rec);
    tuple.t_data = rec;

    Datum *values;
    bool *nulls;
    int natts = tupdesc->natts;

    values = (Datum *) palloc(natts * sizeof(Datum));
    nulls = (bool *) palloc(natts * sizeof(bool));

    heap_deform_tuple(&tuple, tupdesc, values, nulls);

    StringInfoData buf;
    initStringInfo(&buf);

    for (int i = 0; i < natts; i++) {
        if (i > 0)
            appendStringInfoChar(&buf, ',');

        if (nulls[i]) {
            appendStringInfoString(&buf, "");
        } else {
            Oid typOutput;
            bool typIsVarlena;
            getTypeOutputInfo(TupleDescAttr(tupdesc, i)->atttypid, &typOutput, &typIsVarlena);
            char *val = OidOutputFunctionCall(typOutput, values[i]);
            appendStringInfoString(&buf, val);
        }
    }

    ReleaseTupleDesc(tupdesc);

    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

PG_FUNCTION_INFO_V1(csvify_sfunc);

Datum csvify_sfunc(PG_FUNCTION_ARGS)
{
    CsvifyState *state;

    MemoryContext aggctx = NULL;
#if PG_VERSION_NUM >= 90600
    if (!AggCheckCallContext(fcinfo, &aggctx))
        elog(ERROR, "csvify_sfunc called in non-aggregate context");
#else
    aggctx = ((AggState *) fcinfo->context)->ss.ps.state->es_query_cxt;
    if (!aggctx)
        elog(ERROR, "csvify_sfunc called in non-aggregate context");
#endif

if (PG_ARGISNULL(0)) {
    // First call: allocate and initialize state in aggregate context
    MemoryContext oldctx = MemoryContextSwitchTo(aggctx);
    state = (CsvifyState *) palloc0(sizeof(CsvifyState));
    initStringInfo(&state->buf);
    MemoryContextSwitchTo(oldctx);
} else {
    // Subsequent calls: retrieve existing state
    state = (CsvifyState *) PG_GETARG_POINTER(0);
}


    if (state->buf.len > 0) {
        appendStringInfoChar(&state->buf, '\n');
    }

    for (int i = 1; i < PG_NARGS(); i++) {
        if (i > 1) appendStringInfoChar(&state->buf, ',');

        if (PG_ARGISNULL(i)) {
            appendStringInfoString(&state->buf, "");
        } else {
            Oid argtype = get_fn_expr_argtype(fcinfo->flinfo, i);
            Oid typOutput;
            bool typIsVarlena;
            getTypeOutputInfo(argtype, &typOutput, &typIsVarlena);
            char *val = OidOutputFunctionCall(typOutput, PG_GETARG_DATUM(i));
            appendStringInfoString(&state->buf, val);
        }
    }

    PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(csvify_ffunc);
// SELECT col1, csvify(col2, col3) FROM my_table GROUP BY col1;

Datum csvify_ffunc(PG_FUNCTION_ARGS)
{
    CsvifyState *state = (CsvifyState *) PG_GETARG_POINTER(0);
    char *result = pstrdup(state->buf.data);

    pfree(state->buf.data);
    pfree(state);

    PG_RETURN_TEXT_P(cstring_to_text(result));
}
