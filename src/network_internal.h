#ifndef NVR_NETWORK_INTERNAL_H
#define NVR_NETWORK_INTERNAL_H

#include "nvr/network.h"
#include <stdint.h>

#define NVR_NETWORK_MAX_INTERFACES 32
#define NVR_NETWORK_MAX_NEIGHBORS 4096
#define NVR_NETWORK_MAX_HOSTS 1024
#define NVR_NETWORK_DEADLINE_MS 8000

/* Addresses/masks are host order. Kept private to permit deterministic tests
 * of identity verification, deadlines and scan limits without LAN traffic. */
typedef struct {
    uint32_t address;
    uint32_t mask;
    unsigned index;
    intptr_t socket;
} NvrNetworkInterface;

typedef struct {
    uint32_t address;
    unsigned index;
    unsigned char mac[6];
    int reachable;
    uint32_t age;
} NvrNetworkNeighbor;

typedef struct {
    void *context;
    int (*interfaces)(void *, NvrNetworkInterface *, size_t, size_t *);
    int (*neighbors)(void *, const NvrNetworkInterface *, size_t,
                     NvrNetworkNeighbor *, size_t, size_t *);
    int (*probe)(void *, NvrNetworkInterface *, uint32_t);
    uint64_t (*now)(void *);
    void (*pause)(void *, unsigned);
    void (*finish)(void *, NvrNetworkInterface *, size_t);
} NvrNetworkOps;

#ifdef NVR_NETWORK_TESTING
int nvr_network_test_resolve(const NvrNetworkOps *ops, const char *mac,
                             const char *hint, const atomic_int *cancel,
                             char *host, size_t host_size, char *error,
                             size_t error_size, atomic_flag *scan_gate);
#endif

#endif
