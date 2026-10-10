#include "nvr/config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_temp_file.h"

static void write_json(const char *path, const char *json) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs(json, file) >= 0);
    assert(fclose(file) == 0);
}

static void test_mac_normalization(void) {
    char mac[NVR_MAC_MAX];
    const char *valid[] = {"A0:B1:C2:D3:E4:F5", "a0-b1-c2-d3-e4-f5", "A0b1C2d3E4f5"};
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        assert(nvr_mac_normalize(valid[i], mac, sizeof(mac)));
        assert(strcmp(mac, "a0:b1:c2:d3:e4:f5") == 0);
    }
    strcpy(mac, "02-AA-BB-CC-DD-EE");
    assert(nvr_mac_normalize(mac, mac, sizeof(mac)));
    assert(strcmp(mac, "02:aa:bb:cc:dd:ee") == 0);
    const char *invalid[] = {
        "", "00:00:00:00:00:00", "ff:ff:ff:ff:ff:ff", "01:00:5e:00:00:01",
        "a0:b1:c2:d3:e4", "a0:b1:c2:d3:e4:f5:00", "a0:b1-c2:d3:e4:f5",
        "a0:b1:c2:d3:e4:gg", " a0:b1:c2:d3:e4:f5", "a0b1c2d3e4f5 "
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        strcpy(mac, "previous");
        assert(!nvr_mac_normalize(invalid[i], mac, sizeof(mac)));
        assert(mac[0] == '\0');
    }
    assert(!nvr_mac_normalize(NULL, mac, sizeof(mac)));
    assert(!nvr_mac_normalize(valid[0], NULL, sizeof(mac)));
    assert(!nvr_mac_normalize(valid[0], mac, sizeof(mac) - 1));
}

int main(void) {
    test_mac_normalization();
    char path[NVR_TEST_PATH_MAX];
    nvr_test_temp_file(path);
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
    assert(loaded.cameras[0].mac_address[0] == '\0');
    nvr_config_free(&loaded);

    camera.host[0] = '\0';
    strcpy(camera.mac_address, "A0-B1-C2-D3-E4-F5");
    assert(nvr_config_save(path, &original, error, sizeof(error)) == 0);
    assert(nvr_config_load(path, &loaded, error, sizeof(error)) == 0);
    assert(loaded.count == 1 && loaded.cameras[0].host[0] == '\0');
    assert(strcmp(loaded.cameras[0].mac_address, "a0:b1:c2:d3:e4:f5") == 0);
    assert(strcmp(loaded.cameras[0].password_env, "NVR_TEST_PASSWORD") == 0);
    nvr_config_free(&loaded);

    write_json(path, "{\"cameras\":[{\"name\":\"Legada\",\"host\":\"192.168.0.10\"}]}");
    assert(nvr_config_load(path, &loaded, error, sizeof(error)) == 0);
    assert(loaded.cameras[0].mac_address[0] == '\0');
    assert(loaded.cameras[0].port == 554);
    nvr_config_free(&loaded);

    write_json(path, "{\"cameras\":[{\"name\":\"MAC\",\"mac_address\":\"A0B1C2D3E4F5\"}]}");
    assert(nvr_config_load(path, &loaded, error, sizeof(error)) == 0);
    assert(loaded.cameras[0].host[0] == '\0');
    assert(strcmp(loaded.cameras[0].mac_address, "a0:b1:c2:d3:e4:f5") == 0);
    nvr_config_free(&loaded);

    const char *invalid_profiles[] = {
        "{\"cameras\":[{\"name\":\"Sem endereço\",\"host\":\"\",\"mac_address\":\"\"}]}",
        "{\"cameras\":[{\"name\":\"MAC inválido\",\"host\":\"192.168.0.10\",\"mac_address\":\"ff:ff:ff:ff:ff:ff\"}]}",
        "{\"cameras\":[{\"name\":\"Sem nome\",\"mac_address\":\"01:00:5e:00:00:01\"}]}"
    };
    for (size_t i = 0; i < sizeof(invalid_profiles) / sizeof(invalid_profiles[0]); ++i) {
        write_json(path, invalid_profiles[i]);
        assert(nvr_config_load(path, &loaded, error, sizeof(error)) == -1);
        assert(loaded.count == 0 && loaded.cameras == NULL);
    }

    strcpy(camera.mac_address, "ff:ff:ff:ff:ff:ff");
    assert(nvr_config_save(path, &original, error, sizeof(error)) == -1);
    camera.mac_address[0] = '\0';
    assert(nvr_config_save(path, &original, error, sizeof(error)) == -1);
    remove(path);
    return 0;
}
