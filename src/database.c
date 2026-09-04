#include "nvr/database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void set_error(char *error, size_t size, const char *message) {
    if (error && size) snprintf(error, size, "%s", message ? message : "erro SQLite");
}

static int execute(NvrDatabase *database, const char *sql, char *error, size_t size) {
    char *sqlite_error = NULL;
    int result = sqlite3_exec(database->handle, sql, NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) set_error(error, size, sqlite_error);
    sqlite3_free(sqlite_error);
    return result == SQLITE_OK ? 0 : -1;
}

int nvr_database_open(NvrDatabase *database, const char *path, char *error, size_t size) {
    if (!database || !path) return -1;
    memset(database, 0, sizeof(*database));
    if (strlen(path) >= sizeof(database->path)) { set_error(error,size,"caminho do banco muito longo"); return -1; }
    strcpy(database->path, path);
    int result = sqlite3_open_v2(path, &database->handle,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (result != SQLITE_OK) { set_error(error,size,sqlite3_errmsg(database->handle)); nvr_database_close(database); return -1; }
    chmod(path, S_IRUSR | S_IWUSR);
    sqlite3_busy_timeout(database->handle, 3000);
    const char *schema =
        "PRAGMA journal_mode=DELETE;"
        "PRAGMA secure_delete=ON;"
        "CREATE TABLE IF NOT EXISTS cameras ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL, host TEXT NOT NULL, port INTEGER NOT NULL,"
        "username TEXT NOT NULL DEFAULT '', password TEXT NOT NULL DEFAULT '',"
        "grid_path TEXT NOT NULL DEFAULT '/onvif2',"
        "main_path TEXT NOT NULL DEFAULT '/onvif1',"
        "transport TEXT NOT NULL CHECK(transport IN ('udp','tcp')),"
        "position INTEGER NOT NULL DEFAULT 0, enabled INTEGER NOT NULL DEFAULT 1"
        ");";
    if (execute(database, schema, error, size)) { nvr_database_close(database); return -1; }
    return 0;
}

int nvr_database_load(NvrDatabase *database, NvrConfig *config, char *error, size_t size) {
    if (!database || !database->handle || !config) return -1;
    *config = (NvrConfig){0};
    const char *sql = "SELECT name,host,port,username,password,grid_path,main_path,transport "
                      "FROM cameras WHERE enabled=1 ORDER BY position,id LIMIT 9";
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database->handle,sql,-1,&statement,NULL)!=SQLITE_OK) {
        set_error(error,size,sqlite3_errmsg(database->handle)); return -1;
    }
    int result;
    while ((result=sqlite3_step(statement))==SQLITE_ROW) {
        NvrCameraConfig item = {0};
#define COLUMN_TEXT(field,column) do { const unsigned char *v=sqlite3_column_text(statement,column); \
    if(v){size_t n=strlen((const char*)v);if(n>=sizeof(item.field)){result=SQLITE_TOOBIG;goto done;}memcpy(item.field,v,n+1);} } while(0)
        COLUMN_TEXT(name,0); COLUMN_TEXT(host,1); item.port=(unsigned short)sqlite3_column_int(statement,2);
        COLUMN_TEXT(username,3); COLUMN_TEXT(password,4); COLUMN_TEXT(grid_path,5); COLUMN_TEXT(main_path,6);
        const unsigned char *transport=sqlite3_column_text(statement,7);
        item.transport=transport&&!strcmp((const char*)transport,"udp")?NVR_TRANSPORT_UDP:NVR_TRANSPORT_TCP;
#undef COLUMN_TEXT
        NvrCameraConfig *items=realloc(config->cameras,(config->count+1)*sizeof(*items));
        if(!items){result=SQLITE_NOMEM;goto done;} config->cameras=items; config->cameras[config->count++]=item;
    }
done:
    sqlite3_finalize(statement);
    if(result!=SQLITE_DONE){nvr_config_free(config);set_error(error,size,sqlite3_errmsg(database->handle));return -1;}
    return 0;
}

static int bind_camera(sqlite3_stmt *statement, const NvrCameraConfig *camera, int position) {
    int result=SQLITE_OK;
#define BIND_TEXT(index,value) do { if(result==SQLITE_OK) result=sqlite3_bind_text(statement,index,value,-1,SQLITE_TRANSIENT); } while(0)
    BIND_TEXT(1,camera->name); BIND_TEXT(2,camera->host);
    if(result==SQLITE_OK) result=sqlite3_bind_int(statement,3,camera->port);
    BIND_TEXT(4,camera->username); BIND_TEXT(5,camera->password);
    BIND_TEXT(6,camera->grid_path); BIND_TEXT(7,camera->main_path);
    BIND_TEXT(8,nvr_transport_name(camera->transport));
    if(result==SQLITE_OK) result=sqlite3_bind_int(statement,9,position);
#undef BIND_TEXT
    return result;
}

int nvr_database_insert_camera(NvrDatabase *database, const NvrCameraConfig *camera, char *error, size_t size) {
    const char *sql="INSERT INTO cameras(name,host,port,username,password,grid_path,main_path,transport,position) "
                    "VALUES(?,?,?,?,?,?,?,?,(SELECT count(*) FROM cameras))";
    sqlite3_stmt *statement=NULL;
    if(sqlite3_prepare_v2(database->handle,sql,-1,&statement,NULL)!=SQLITE_OK){set_error(error,size,sqlite3_errmsg(database->handle));return -1;}
    int result=bind_camera(statement,camera,0);
    if(result==SQLITE_OK) result=sqlite3_step(statement);
    sqlite3_finalize(statement); chmod(database->path,S_IRUSR|S_IWUSR);
    if(result!=SQLITE_DONE){set_error(error,size,sqlite3_errmsg(database->handle));return -1;} return 0;
}

int nvr_database_replace_all(NvrDatabase *database, const NvrConfig *config, char *error, size_t size) {
    if(execute(database,"BEGIN IMMEDIATE; DELETE FROM cameras;",error,size)) return -1;
    const char *sql="INSERT INTO cameras(name,host,port,username,password,grid_path,main_path,transport,position) VALUES(?,?,?,?,?,?,?,?,?)";
    sqlite3_stmt *statement=NULL; int failed=0;
    if(sqlite3_prepare_v2(database->handle,sql,-1,&statement,NULL)!=SQLITE_OK) failed=1;
    for(size_t i=0;!failed&&i<config->count;i++) {
        sqlite3_reset(statement); sqlite3_clear_bindings(statement);
        if(bind_camera(statement,&config->cameras[i],(int)i)!=SQLITE_OK||sqlite3_step(statement)!=SQLITE_DONE) failed=1;
    }
    sqlite3_finalize(statement);
    if(failed){execute(database,"ROLLBACK;",NULL,0);set_error(error,size,sqlite3_errmsg(database->handle));return -1;}
    if(execute(database,"COMMIT;",error,size)) return -1;
    chmod(database->path,S_IRUSR|S_IWUSR); return 0;
}

void nvr_database_close(NvrDatabase *database) {
    if(!database)return; if(database->handle)sqlite3_close(database->handle); memset(database,0,sizeof(*database));
}
