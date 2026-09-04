#include "nvr/database.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void){
    char path[]="/tmp/nvr-database-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);unlink(path);
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
    assert(nvr_database_delete_camera(&database,0,error,sizeof(error))==0);
    assert(nvr_database_load(&database,&loaded,error,sizeof(error))==0&&loaded.count==0);nvr_config_free(&loaded);
    nvr_database_close(&database);unlink(path);return 0;
}
