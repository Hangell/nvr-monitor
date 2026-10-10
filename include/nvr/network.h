#ifndef NVR_NETWORK_H
#define NVR_NETWORK_H

#include <stddef.h>
#include <stdatomic.h>

/* Resolve a camera's MAC on directly connected IPv4 Ethernet/Wi-Fi LANs.
 * Numeric host hints are probed first, but never accepted without matching the
 * MAC in a confirmed OS neighbor entry. Hostname hints do not perform DNS.
 * No administrator privileges or external commands are needed. Discovery is
 * limited to physical LAN adapters, 1,024 hosts total and an 8-second deadline.
 * Concurrent callers share a cancellable scan lock. No mapping is cached here.
 * Returns 0 on success, -1 with an explanatory error otherwise. NULL cancel
 * is allowed. Successful host is an IPv4 address (at least 16 bytes required).
 */
int nvr_network_resolve_mac(const char *mac, const char *host_hint,
                            const atomic_int *cancel, char *host,
                            size_t host_size, char *error, size_t error_size);

#endif
