#include "cool_c_ext.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

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
