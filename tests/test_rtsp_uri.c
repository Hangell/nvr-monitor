#include "nvr/rtsp.h"
#include <assert.h>
#include <string.h>

int main(void) {
    char encoded[64], uri[512];
    assert(nvr_rtsp_percent_encode("a@b:c/ d", encoded, sizeof(encoded)) == 0);
    assert(strcmp(encoded, "a%40b%3Ac%2F%20d") == 0);
    NvrCameraConfig c = {.port = 554, .transport = NVR_TRANSPORT_UDP};
    strcpy(c.host, "10.0.0.2"); strcpy(c.username, "user@example.com");
    strcpy(c.grid_path, "/onvif2"); strcpy(c.main_path, "/onvif1");
    assert(nvr_rtsp_build_uri(&c, 0, "p@ss:word", uri, sizeof(uri)) == 0);
    assert(strcmp(uri, "rtsp://user%40example.com:p%40ss%3Aword@10.0.0.2:554/onvif2") == 0);
    assert(nvr_rtsp_build_uri(&c, 1, "p@ss:word", uri, sizeof(uri)) == 0);
    assert(strstr(uri, "/onvif1") != NULL);
    return 0;
}
