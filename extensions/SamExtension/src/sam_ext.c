#include "sam_ext.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(join);

// select join(array['This', NULL, 'Is', 'An', 'Array', 'please join it']);
Datum join(PG_FUNCTION_ARGS)
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
            appendStringInfoChar(&result, ' ');
        }

        if (!nulls[i])
        {
            element = DatumGetTextP(elements[i]);
            appendStringInfoString(&result, text_to_cstring(element));
        }
        else
        {
            appendStringInfoString(&result, "<null>");
        }
    }

    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

PG_FUNCTION_INFO_V1(sptrim);

// select sptrim('  please trim this string  ')
Datum sptrim(PG_FUNCTION_ARGS)
{
    text *input_text = PG_GETARG_TEXT_P(0);
    char *input_str = text_to_cstring(input_text);
    int input_len = strlen(input_str);
    int i;
    char *new_str = input_str;
    int new_str_len = input_len;
    text *result_text;

    i = 0;
    while (i < input_len && input_str[i] == ' ') {
        new_str++;
        new_str_len--;
        i++;
    }

    i = new_str_len - 1;
    while (i >= 0 && new_str[i] == ' ') {
        new_str[i] = '\0';
        i--;
    }

    result_text = cstring_to_text(new_str);
    PG_RETURN_TEXT_P(result_text);
}
