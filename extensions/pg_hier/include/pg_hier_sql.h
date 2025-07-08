#ifndef PG_HIER_SQL_H
#define PG_HIER_SQL_H

#define PG_HIER_SQL_GET_HIER_BY_ID \
        "SELECT parent_name, parent_key, name, child_key FROM pg_hier_detail " \
        "WHERE HIERARCHY_ID = $1 " \
        "    AND LEVEL >= (SELECT LEVEL FROM PG_HIER_DETAIL WHERE PARENT_NAME = $2) " \
        "    AND LEVEL <= (SELECT LEVEL FROM PG_HIER_DETAIL WHERE NAME = $3) " \
        "ORDER BY LEVEL DESC" 

#endif