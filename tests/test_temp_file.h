#ifndef NVR_TEST_TEMP_FILE_H
#define NVR_TEST_TEMP_FILE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#define NVR_TEST_PATH_MAX MAX_PATH
#else
#include <unistd.h>
#define NVR_TEST_PATH_MAX 512
#endif

/* Create a real, unique file in the native OS temporary directory. */
static void nvr_test_temp_file(char path[NVR_TEST_PATH_MAX]) {
#ifdef _WIN32
    char directory[MAX_PATH];
    DWORD length = GetTempPathA(MAX_PATH, directory);
    assert(length > 0 && length < MAX_PATH);
    assert(GetTempFileNameA(directory, "nvr", 0, path) != 0);
#else
    const char *directory = getenv("TMPDIR");
    if (!directory || !directory[0]) directory = "/tmp";
    int length = snprintf(path, NVR_TEST_PATH_MAX, "%s/nvr-test-XXXXXX", directory);
    assert(length > 0 && length < NVR_TEST_PATH_MAX);
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
#endif
}
#endif
