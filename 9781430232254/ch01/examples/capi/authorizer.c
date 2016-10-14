#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/util.h"

/* Purpose: Illstrate use of sqlite3_set_authorizer() to control
**          SQL execution.
**
** Program does the following:
**
**    - Create a table foo(x,y,z) and perform various operations on it. 
**    - Watch database events as operations are performed
**    - Control various database events:
**        1. Prohibit updates to column x
**        2. Prohibit reading of column z
*/

int auth( void* x, int type, 
          const char* a, const char* b, 
          const char* c, const char* d );

int main(int argc, char **argv)
{
    sqlite3 *db, *db2;
    char *zErr;
    int rc;

    /* -------------------------------------------------------------------------
    **  Setup                                                                  
    ** -------------------------------------------------------------------------
    */

    /* Connect to test.db */
    rc = sqlite3_open("test.db", &db);

    if(rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    /* -------------------------------------------------------------------------
    **  Authorize and test
    ** -------------------------------------------------------------------------
    */

    /* Register the authorizer function */
    sqlite3_set_authorizer(db, auth, NULL);

    /* Test transactions events */

    printf("program : Starting transaction\n");
    sqlite3_exec(db, "BEGIN", NULL, NULL, &zErr);

    printf("program : Committing transaction\n");
    sqlite3_exec(db, "COMMIT", NULL, NULL, &zErr);

    printf("program : Starting transaction\n");
    sqlite3_exec(db, "BEGIN", NULL, NULL, &zErr);

    printf("program : Aborting transaction\n");
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, &zErr);

    /* Test table events */

    printf("program : Creating table\n");
    sqlite3_exec(db, "create table foo(x int, y int, z int)", NULL, NULL, &zErr);

    printf("program : Inserting record\n");
    sqlite3_exec(db, "insert into foo values (1,2,3)", NULL, NULL, &zErr);

    printf("program : Selecting record (value for z should be NULL)\n");
    print_sql_result(db, "select * from foo");

    printf("program : Updating record (update of x should be denied)\n");
    sqlite3_exec(db, "update foo set x=4, y=5, z=6", NULL, NULL, &zErr);

    printf("program : Selecting record (notice x was not updated)\n");
    print_sql_result(db, "select * from foo");

    printf("program : Deleting record\n");
    sqlite3_exec(db, "delete from foo", NULL, NULL, &zErr);

    printf("program : Dropping table\n");
    sqlite3_exec(db, "drop table foo", NULL, NULL, &zErr);

    /* Test ATTACH/DETACH */

    /* Connect to test2.db */
    rc = sqlite3_open("test2.db", &db2);

    if(rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db2));
        sqlite3_close(db2);
        exit(1);
     }

    /* Drop table foo2 in test2 if exists */
    sqlite3_exec(db2, "drop table foo2", NULL, NULL, &zErr);
    sqlite3_exec(db2, "create table foo2(x int, y int, z int)", NULL, NULL, &zErr);

    /* Attach database test2.db to test.db */
    printf("program : Attaching database test2.db\n");
    sqlite3_exec(db, "attach 'test2.db' as test2", NULL, NULL, &zErr);

    /* Select record from test2.db foo2 in test.db */
    printf("program : Selecting record from attached database test2.db\n");
    sqlite3_exec(db, "select * from foo2", NULL, NULL, &zErr);

    printf("program : Detaching table\n");
    sqlite3_exec(db, "detach test2", NULL, NULL, &zErr);

    /* -------------------------------------------------------------------------
    **  Cleanup
    ** -------------------------------------------------------------------------
    */
    
    sqlite3_close(db);
    sqlite3_close(db2);

    return 0;    
}
 
int auth( void* x, int type, 
          const char* a, const char* b, 
          const char* c, const char* d )
{
    const char* operation = a;
    
    printf( "    %s ", event_description(type));

    if((a != NULL) && (type == SQLITE_TRANSACTION)) {
        printf(": %s Transaction", operation);
    }

    switch(type) {
        case SQLITE_CREATE_INDEX:
        case SQLITE_CREATE_TABLE:
        case SQLITE_CREATE_TRIGGER:
        case SQLITE_CREATE_VIEW:
        case SQLITE_DROP_INDEX:
        case SQLITE_DROP_TABLE:
        case SQLITE_DROP_TRIGGER:
        case SQLITE_DROP_VIEW:
        {
            printf(": Schema modified");
        }
    }

    if(type == SQLITE_READ) {

        printf(": Read of %s.%s ", a, b);

        /* Block attempts to read column z */
        if(strcmp(b,"z")==0) {
            printf("-> DENIED\n");
            return SQLITE_IGNORE;
        }
    }

    if(type == SQLITE_INSERT) {
        printf(": Insert records into %s ", a);
    }

    if(type == SQLITE_UPDATE) {
        printf(": Update of %s.%s ", a, b);

        /* Block updates of column x */
        if(strcmp(b,"x")==0) {
            printf("-> DENIED\n");
            return SQLITE_IGNORE;
        }
    }

    if(type == SQLITE_DELETE) {
        printf(": Delete from %s ", a);
    }

    if(type == SQLITE_ATTACH) {
        printf(": %s", a);
    }

    if(type == SQLITE_DETACH) {
        printf("-> %s", a);
    }

    printf("\n");

    return SQLITE_OK;
}

