#include "nvr/config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[] = "/tmp/nvr-config-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); close(fd);
    NvrCameraConfig camera = {.port = 8554, .transport = NVR_TRANSPORT_UDP};
    strcpy(camera.name, "Portão"); strcpy(camera.host, "192.168.0.10");
    strcpy(camera.username, "admin"); strcpy(camera.password_env, "NVR_TEST_PASSWORD");
    strcpy(camera.grid_path, "/onvif2"); strcpy(camera.main_path, "/onvif1");
    NvrConfig original = {&camera, 1}; char error[128];
    assert(nvr_config_save(path, &original, error, sizeof(error)) == 0);
    NvrConfig loaded;
    assert(nvr_config_load(path, &loaded, error, sizeof(error)) == 0);
    assert(loaded.count == 1 && loaded.cameras[0].port == 8554);
    assert(loaded.cameras[0].transport == NVR_TRANSPORT_UDP);
    assert(strcmp(loaded.cameras[0].password_env, "NVR_TEST_PASSWORD") == 0);
    nvr_config_free(&loaded); unlink(path);
    return 0;
}
