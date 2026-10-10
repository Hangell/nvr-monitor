#include "../src/network_internal.h"
#include <assert.h>
#include <string.h>

static const char *camera_mac = "02:12:34:56:78:90";
static const unsigned char camera_bytes[6] = {2, 0x12, 0x34, 0x56, 0x78, 0x90};

typedef struct {
    NvrNetworkInterface interface;
    NvrNetworkNeighbor entries[8];
    size_t count;
    uint64_t now;
    uint32_t camera_address;
    uint64_t reachable_after;
    int camera_probed;
    unsigned probes;
    unsigned reads;
    unsigned enumerations;
    unsigned finishes;
    int table_failure;
    atomic_int *cancel;
    atomic_flag *gate;
    uint64_t gate_release;
    int publish_after_gate;
} Fixture;

static Fixture fixture(void) {
    Fixture f = {0};
    f.interface = (NvrNetworkInterface){UINT32_C(0xc0a80164), UINT32_C(0xffffff00), 7, -1};
    return f;
}

static void add_neighbor(Fixture *f, uint32_t address, unsigned index, int reachable) {
    assert(f->count < 8);
    NvrNetworkNeighbor *entry = &f->entries[f->count++];
    entry->address = address;
    entry->index = index;
    memcpy(entry->mac, camera_bytes, 6);
    entry->reachable = reachable;
}

static int interfaces(void *context, NvrNetworkInterface *items, size_t capacity, size_t *count) {
    Fixture *f = context;
    assert(capacity >= 1);
    f->enumerations++;
    items[0] = f->interface;
    *count = 1;
    return 0;
}

static int neighbors(void *context, const NvrNetworkInterface *items, size_t item_count,
                      NvrNetworkNeighbor *out, size_t capacity, size_t *count) {
    Fixture *f = context;
    assert(item_count == 1 && items[0].index == 7);
    f->reads++;
    if (f->table_failure) return -1;
    assert(capacity > f->count);
    memcpy(out, f->entries, f->count * sizeof(*out));
    *count = f->count;
    if (f->camera_address && f->camera_probed && f->now >= f->reachable_after) {
        NvrNetworkNeighbor *entry = &out[(*count)++];
        *entry = (NvrNetworkNeighbor){.address = f->camera_address, .index = 7, .reachable = 1};
        memcpy(entry->mac, camera_bytes, 6);
    }
    return 0;
}

static int probe(void *context, NvrNetworkInterface *interface, uint32_t address) {
    Fixture *f = context;
    uint32_t network = interface->address & interface->mask;
    assert((address & interface->mask) == network);
    assert(address != network && address != (network | ~interface->mask));
    assert(address != interface->address);
    f->probes++;
    if (address == f->camera_address) f->camera_probed = 1;
    return 0;
}

static uint64_t now(void *context) { return ((Fixture *)context)->now; }

static void pause_time(void *context, unsigned milliseconds) {
    Fixture *f = context;
    f->now += milliseconds;
    if (f->cancel && f->now >= 80) atomic_store(f->cancel, 1);
    if (f->gate && f->now >= f->gate_release) {
        atomic_flag_clear(f->gate);
        f->gate = NULL;
        if (f->publish_after_gate) f->camera_probed = 1;
    }
}

static void finish(void *context, NvrNetworkInterface *items, size_t count) {
    Fixture *f = context;
    assert(count == 1 && items[0].index == 7);
    f->finishes++;
}

static int resolve(Fixture *f, const char *mac, const char *hint, const atomic_int *cancel,
                   char host[16], char error[256], atomic_flag *gate) {
    NvrNetworkOps ops = {f, interfaces, neighbors, probe, now, pause_time, finish};
    return nvr_network_test_resolve(&ops, mac, hint, cancel, host, 16, error, 256, gate);
}

int main(void) {
    char host[16] = "previous", error[256] = "previous";
    Fixture f = fixture();
    add_neighbor(&f, UINT32_C(0xc0a8012a), 7, 1);
    assert(resolve(&f, camera_mac, "192.168.1.9", NULL, host, error, NULL) == 0);
    assert(strcmp(host, "192.168.1.42") == 0 && !error[0]);
    assert(f.probes == 0 && f.finishes == 1);
    /* Prefer the most recently confirmed mapping when the OS has two entries. */
    f = fixture();
    add_neighbor(&f, UINT32_C(0xc0a8012a), 7, 1);
    add_neighbor(&f, UINT32_C(0xc0a8012b), 7, 1);
    f.entries[0].age = 900;
    f.entries[1].age = 5;
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) == 0);
    assert(strcmp(host, "192.168.1.43") == 0);

    /* A hint belonging to a different camera is never an IP fallback. */
    f = fixture();
    add_neighbor(&f, UINT32_C(0xc0a80109), 7, 1);
    f.entries[0].mac[5] = 0x91;
    assert(resolve(&f, camera_mac, "192.168.1.9", NULL, host, error, NULL) < 0);
    assert(!host[0] && strstr(error, "nao encontrada"));
    assert(f.now <= NVR_NETWORK_DEADLINE_MS + 40);
    assert(f.probes == 254); /* 253 remote hosts plus the numeric hint. */

    /* An old DHCP mapping remains stale; discovery finds the new IP. */
    f = fixture();
    add_neighbor(&f, UINT32_C(0xc0a80115), 7, 0);
    f.camera_address = UINT32_C(0xc0a801c8);
    assert(resolve(&f, camera_mac, "192.168.1.21", NULL, host, error, NULL) == 0);
    assert(strcmp(host, "192.168.1.200") == 0 && f.camera_probed);
    /* Reconnect reads the OS table again instead of reusing an app cache. */
    f = fixture();
    add_neighbor(&f, UINT32_C(0xc0a801c9), 7, 1);
    assert(resolve(&f, camera_mac, "192.168.1.200", NULL, host, error, NULL) == 0);
    assert(strcmp(host, "192.168.1.201") == 0);

    /* A stale entry is usable only after the kernel confirms reachability. */
    f = fixture();
    f.camera_address = UINT32_C(0xc0a8012a);
    f.reachable_after = 5000;
    add_neighbor(&f, f.camera_address, 7, 0);
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) == 0);
    assert(f.now >= 5000 && f.now < NVR_NETWORK_DEADLINE_MS);

    /* Neighbors from VPN interfaces, routed IPs and local/broadcast addresses
     * cannot satisfy identity even when their MAC bytes match. */
    f = fixture();
    add_neighbor(&f, UINT32_C(0xc0a8012a), 8, 1);
    add_neighbor(&f, UINT32_C(0x0a01022a), 7, 1);
    add_neighbor(&f, UINT32_C(0xc0a801ff), 7, 1);
    add_neighbor(&f, f.interface.address, 7, 1);
    assert(resolve(&f, camera_mac, "10.1.2.42", NULL, host, error, NULL) < 0);
    assert(f.probes == 253 && !host[0]);

    /* Wide LANs permit a verified table hit or a single local hint, and must
     * not silently claim that a truncated broad scan covers the whole LAN. */
    f = fixture();
    f.interface.mask = UINT32_C(0xffff0000);
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) < 0);
    assert(strstr(error, "1024") && f.probes == 0 && f.now == 0);
    f = fixture();
    f.interface.mask = UINT32_C(0xffff0000);
    f.camera_address = UINT32_C(0xc0a8022a);
    assert(resolve(&f, camera_mac, "192.168.2.42", NULL, host, error, NULL) == 0);
    assert(strcmp(host, "192.168.2.42") == 0 && f.probes == 1);
    f = fixture();
    f.interface.mask = UINT32_C(0xffff0000);
    assert(resolve(&f, camera_mac, "192.168.2.42", NULL, host, error, NULL) < 0);
    assert(strstr(error, "1024") && !host[0] && f.probes == 1);
    f = fixture();
    f.interface.mask = UINT32_C(0xffff0000);
    f.camera_address = UINT32_C(0xc0a8022a);
    f.reachable_after = 5000;
    add_neighbor(&f, f.camera_address, 7, 0);
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) == 0);
    assert(strcmp(host, "192.168.2.42") == 0 && f.probes == 2 && f.now >= 5000);
    f = fixture();
    f.interface.mask = UINT32_C(0xfffffc00); /* /22: 1,024 addresses, 1,021 peers. */
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) < 0);
    assert(f.probes == 1021 && strstr(error, "nao encontrada"));
    f = fixture();
    f.interface.mask = UINT32_C(0xfffff800); /* /21: explicit failure, no partial scan. */
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) < 0);
    assert(f.probes == 0 && strstr(error, "1024"));

    /* A concurrent scan may populate the table while this caller waits. */
    atomic_flag gate = ATOMIC_FLAG_INIT;
    assert(!atomic_flag_test_and_set(&gate));
    f = fixture();
    f.gate = &gate;
    f.gate_release = 100;
    f.publish_after_gate = 1;
    f.camera_address = UINT32_C(0xc0a8012a);
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, &gate) == 0);
    assert(f.probes == 0 && f.now == 100);
    assert(!atomic_flag_test_and_set(&gate));
    atomic_flag_clear(&gate);
    assert(!atomic_flag_test_and_set(&gate));
    f = fixture();
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, &gate) < 0);
    assert(f.now == NVR_NETWORK_DEADLINE_MS && f.probes == 0);
    assert(atomic_flag_test_and_set(&gate));
    atomic_flag_clear(&gate);

    atomic_int cancel;
    atomic_init(&cancel, 1);
    f = fixture();
    assert(resolve(&f, camera_mac, NULL, &cancel, host, error, NULL) < 0);
    assert(strstr(error, "cancelada") && f.enumerations == 0);
    atomic_store(&cancel, 0);
    f = fixture();
    f.cancel = &cancel;
    assert(resolve(&f, camera_mac, NULL, &cancel, host, error, &gate) < 0);
    assert(strstr(error, "cancelada") && f.now == 80 && !host[0]);
    assert(!atomic_flag_test_and_set(&gate));
    atomic_flag_clear(&gate);
    atomic_store(&cancel, 0);
    f = fixture();
    f.cancel = &cancel;
    assert(!atomic_flag_test_and_set(&gate));
    assert(resolve(&f, camera_mac, NULL, &cancel, host, error, &gate) < 0);
    assert(strstr(error, "cancelada") && f.probes == 0 && f.now == 100);
    assert(atomic_flag_test_and_set(&gate)); /* Does not release another scan's lock. */
    atomic_flag_clear(&gate);

    f = fixture();
    f.table_failure = 1;
    assert(resolve(&f, camera_mac, NULL, NULL, host, error, NULL) < 0);
    assert(strstr(error, "tabela de vizinhos") && f.finishes == 1);
    f = fixture();
    assert(resolve(&f, "ff:ff:ff:ff:ff:ff", NULL, NULL, host, error, NULL) < 0);
    assert(strstr(error, "invalido") && f.enumerations == 0);
    /* These public calls fail before any native network enumeration/probe. */
    assert(nvr_network_resolve_mac(camera_mac, NULL, NULL, host, 3, error, sizeof(error)) < 0);
    assert(strstr(error, "Buffer"));
    assert(nvr_network_resolve_mac(NULL, NULL, NULL, host, sizeof(host), error, sizeof(error)) < 0);
    assert(strstr(error, "invalido"));
    return 0;
}
