#include "nvr/database.h"
#include <assert.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "test_temp_file.h"

static int scalar(sqlite3 *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    assert(sqlite3_prepare_v2(database,sql,-1,&statement,NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    int value = sqlite3_column_int(statement,0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return value;
}

static void test_legacy_migration(void) {
    char path[NVR_TEST_PATH_MAX];
    nvr_test_temp_file(path);
    sqlite3 *legacy = NULL;
    assert(sqlite3_open(path,&legacy) == SQLITE_OK);
    const char *schema =
        "CREATE TABLE cameras (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,host TEXT NOT NULL,port INTEGER NOT NULL,"
        "username TEXT NOT NULL DEFAULT '',password TEXT NOT NULL DEFAULT '',password_env TEXT NOT NULL DEFAULT '',"
        "grid_path TEXT NOT NULL DEFAULT '/onvif2',main_path TEXT NOT NULL DEFAULT '/onvif1',"
        "transport TEXT NOT NULL CHECK(transport IN ('udp','tcp')),"
        "position INTEGER NOT NULL DEFAULT 0,enabled INTEGER NOT NULL DEFAULT 1);"
        "INSERT INTO cameras(id,name,host,port,username,password,password_env,grid_path,main_path,transport,position,enabled) VALUES"
        "(7,'Entrada','192.168.0.10',8554,'admin','legado','NVR_LEGACY_PASSWORD','/grid','/main','udp',5,1),"
        "(3,'Quintal','192.168.0.11',554,'viewer','','','/onvif2','/onvif1','tcp',1,1),"
        "(9,'Desativada','192.168.0.12',554,'','','','/onvif2','/onvif1','tcp',0,0);"
        "PRAGMA user_version=1;";
    assert(sqlite3_exec(legacy,schema,NULL,NULL,NULL) == SQLITE_OK);
    assert(sqlite3_close(legacy) == SQLITE_OK);

    NvrDatabase database;
    char error[256];
    assert(nvr_database_open(&database,path,error,sizeof(error)) == 0);
    assert(scalar(database.handle,"PRAGMA user_version") == 2);
    assert(scalar(database.handle,"SELECT count(*) FROM cameras") == 3);
    assert(scalar(database.handle,"SELECT count(*) FROM cameras WHERE mac_address=''") == 3);
    assert(scalar(database.handle,"SELECT position FROM cameras WHERE id=7") == 5);
    assert(scalar(database.handle,"SELECT enabled FROM cameras WHERE id=9") == 0);
    NvrConfig loaded;
    assert(nvr_database_load(&database,&loaded,error,sizeof(error)) == 0);
    assert(loaded.count == 2);
    assert(strcmp(loaded.cameras[0].name,"Quintal") == 0);
    assert(strcmp(loaded.cameras[1].host,"192.168.0.10") == 0);
    assert(strcmp(loaded.cameras[1].username,"admin") == 0);
    assert(strcmp(loaded.cameras[1].password,"legado") == 0);
    assert(strcmp(loaded.cameras[1].password_env,"NVR_LEGACY_PASSWORD") == 0);
    assert(strcmp(loaded.cameras[1].grid_path,"/grid") == 0);
    assert(strcmp(loaded.cameras[1].main_path,"/main") == 0);
    assert(loaded.cameras[1].port == 8554 && loaded.cameras[1].transport == NVR_TRANSPORT_UDP);
    assert(loaded.cameras[1].mac_address[0] == '\0');
    strcpy(loaded.cameras[1].mac_address,"A0B1C2D3E4F5");
    assert(nvr_database_update_camera(&database,1,&loaded.cameras[1],error,sizeof(error)) == 0);
    nvr_config_free(&loaded);
    nvr_database_close(&database);

    assert(nvr_database_open(&database,path,error,sizeof(error)) == 0);
    assert(scalar(database.handle,"SELECT count(*) FROM cameras") == 3);
    assert(nvr_database_load(&database,&loaded,error,sizeof(error)) == 0);
    assert(strcmp(loaded.cameras[1].mac_address,"a0:b1:c2:d3:e4:f5") == 0);
    assert(strcmp(loaded.cameras[1].password,"legado") == 0);
    nvr_config_free(&loaded);
    nvr_database_close(&database);
    remove(path);
}

int main(void){
    test_legacy_migration();
    char path[NVR_TEST_PATH_MAX];
    nvr_test_temp_file(path);
    NvrDatabase database;char error[256];assert(nvr_database_open(&database,path,error,sizeof(error))==0);
    NvrCameraConfig camera={.port=554,.transport=NVR_TRANSPORT_UDP};
    strcpy(camera.name,"Entrada");strcpy(camera.host,"192.168.0.10");strcpy(camera.username,"admin");
    strcpy(camera.password,"segredo");strcpy(camera.grid_path,"/onvif2");strcpy(camera.main_path,"/onvif1");
    assert(nvr_database_insert_camera(&database,&camera,error,sizeof(error))==0);
    NvrConfig loaded;assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0);assert(loaded.count==1);
    assert(strcmp(loaded.cameras[0].password,"segredo")==0);
    strcpy(camera.host,"192.168.0.20");assert(nvr_database_update_camera(&database,0,&camera,error,sizeof(error))==0);
    nvr_config_free(&loaded);assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0);
    assert(strcmp(loaded.cameras[0].host,"192.168.0.20")==0);nvr_config_free(&loaded);

    camera.host[0]='\0';strcpy(camera.mac_address,"A0-B1-C2-D3-E4-F5");
    assert(nvr_database_update_camera(&database,0,&camera,error,sizeof(error))==0);
    assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0);
    assert(loaded.cameras[0].host[0]=='\0');
    assert(strcmp(loaded.cameras[0].mac_address,"a0:b1:c2:d3:e4:f5")==0);
    assert(strcmp(loaded.cameras[0].password,"segredo")==0);nvr_config_free(&loaded);

    strcpy(camera.mac_address,"ff:ff:ff:ff:ff:ff");
    assert(nvr_database_insert_camera(&database,&camera,error,sizeof(error))==-1);
    assert(nvr_database_update_camera(&database,0,&camera,error,sizeof(error))==-1);
    NvrConfig replacement={&camera,1};
    assert(nvr_database_replace_all(&database,&replacement,error,sizeof(error))==-1);
    assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0&&loaded.count==1);
    assert(strcmp(loaded.cameras[0].mac_address,"a0:b1:c2:d3:e4:f5")==0);nvr_config_free(&loaded);

    strcpy(camera.mac_address,"02:aa:bb:cc:dd:ee");
    assert(nvr_database_replace_all(&database,&replacement,error,sizeof(error))==0);
    assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0&&loaded.count==1);
    assert(strcmp(loaded.cameras[0].mac_address,"02:aa:bb:cc:dd:ee")==0);nvr_config_free(&loaded);
    assert(nvr_database_delete_camera(&database,0,error,sizeof(error))==0);
    assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0&&loaded.count==0);nvr_config_free(&loaded);
    nvr_database_close(&database);remove(path);return 0;
}
