struct string_array {
    char **data;
    int size;
    int capacity;
};

struct table_stack {
    char *table_name;
    struct table_stack *next;
};