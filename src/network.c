#ifndef _WIN32
#define _DEFAULT_SOURCE 1
#endif
#include "nvr/network.h"
#include "nvr/config.h"
#include "network_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#elif defined(__linux__)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

static atomic_flag scan_gate = ATOMIC_FLAG_INIT;

static int set_error(char *error, size_t size, const char *message) {
    if (error && size) snprintf(error, size, "%s", message);
    return -1;
}

static int cancelled(const atomic_int *cancel) {
    return cancel && atomic_load(cancel);
}

static int parse_ipv4(const char *text, uint32_t *address) {
    unsigned octets[4];
    char extra;
    if (!text || sscanf(text, "%u.%u.%u.%u%c", &octets[0], &octets[1],
                        &octets[2], &octets[3], &extra) != 4) return 0;
    for (unsigned i = 0; i < 4; ++i) if (octets[i] > 255) return 0;
    *address = (octets[0] << 24) | (octets[1] << 16) |
               (octets[2] << 8) | octets[3];
    return 1;
}

static int direct_host(const NvrNetworkInterface *interface, uint32_t address) {
    uint32_t network = interface->address & interface->mask;
    uint32_t broadcast = network | ~interface->mask;
    return address != interface->address && address != network &&
           address != broadcast && (address & interface->mask) == network &&
           address < UINT32_C(0xe0000000) && (address >> 24) != 127 &&
           (address >> 24) != 0;
}

static NvrNetworkInterface *find_interface(NvrNetworkInterface *interfaces,
                                          size_t count, unsigned index,
                                          uint32_t address) {
    for (size_t i = 0; i < count; ++i)
        if (interfaces[i].index == index && direct_host(&interfaces[i], address))
            return &interfaces[i];
    return NULL;
}

static int store_host(uint32_t address, char *host, size_t size) {
    int written = snprintf(host, size, "%u.%u.%u.%u", address >> 24,
                           (address >> 16) & 255, (address >> 8) & 255,
                           address & 255);
    return written >= 0 && (size_t)written < size ? 0 : -1;
}

static int read_matches(const NvrNetworkOps *ops,
                        NvrNetworkInterface *interfaces, size_t count,
                        NvrNetworkNeighbor *neighbors, const unsigned char mac[6],
                        int probe_stale, char *host, size_t size) {
    size_t neighbor_count = 0;
    if (ops->neighbors(ops->context, interfaces, count, neighbors,
                       NVR_NETWORK_MAX_NEIGHBORS, &neighbor_count) < 0) return -1;
    const NvrNetworkNeighbor *best = NULL;
    for (size_t i = 0; i < neighbor_count; ++i) {
        NvrNetworkNeighbor *entry = &neighbors[i];
        if (memcmp(entry->mac, mac, 6)) continue;
        NvrNetworkInterface *interface = find_interface(interfaces, count,
                                                       entry->index, entry->address);
        if (!interface) continue;
        if (entry->reachable) {
            if (!best || entry->age < best->age) best = entry;
        } else if (probe_stale) {
            ops->probe(ops->context, interface, entry->address);
        }
    }
    if (!best) return 0;
    return store_host(best->address, host, size) == 0 ? 1 : -1;
}

static int resolve_with_ops(const NvrNetworkOps *ops, const char *mac_text,
                            const char *hint, const atomic_int *cancel,
                            char *host, size_t host_size, char *error,
                            size_t error_size, atomic_flag *gate) {
    char canonical[NVR_MAC_MAX];
    if (host && host_size) host[0] = '\0';
    if (error && error_size) error[0] = '\0';
    if (!host || host_size < 16)
        return set_error(error, error_size, "Buffer insuficiente para o IP da camera.");
    if (!nvr_mac_normalize(mac_text, canonical, sizeof(canonical)))
        return set_error(error, error_size, "MAC da camera invalido.");
    if (cancelled(cancel))
        return set_error(error, error_size, "Busca por MAC cancelada.");
    unsigned octets[6];
    sscanf(canonical, "%x:%x:%x:%x:%x:%x", &octets[0], &octets[1],
           &octets[2], &octets[3], &octets[4], &octets[5]);
    unsigned char mac[6];
    for (size_t i = 0; i < 6; ++i) mac[i] = (unsigned char)octets[i];
    NvrNetworkInterface interfaces[NVR_NETWORK_MAX_INTERFACES] = {{0}};
    size_t count = 0;
    if (ops->interfaces(ops->context, interfaces, NVR_NETWORK_MAX_INTERFACES, &count) < 0)
        return set_error(error, error_size, "Nao foi possivel consultar as interfaces da rede local.");
    if (!count)
        return set_error(error, error_size, "MAC exige uma rede IPv4 local em Ethernet ou Wi-Fi fisico; use IP para redes roteadas/VPN.");
    NvrNetworkNeighbor *neighbors = calloc(NVR_NETWORK_MAX_NEIGHBORS, sizeof(*neighbors));
    if (!neighbors) {
        ops->finish(ops->context, interfaces, count);
        return set_error(error, error_size, "Memoria insuficiente para consultar a rede local.");
    }
    uint64_t deadline = ops->now(ops->context) + NVR_NETWORK_DEADLINE_MS;
    int result = -1, owns_gate = 0, oversized = 0, sent = 0;
    const char *failure = "Camera MAC nao encontrada na rede IPv4 local; confirme o MAC, a conexao e a mesma LAN.";
    int match = read_matches(ops, interfaces, count, neighbors, mac, 1, host, host_size);
    if (match > 0) { result = 0; goto done; }
    if (match < 0) { failure = "Nao foi possivel consultar a tabela de vizinhos da rede."; goto done; }

    uint32_t hinted_ip;
    int targeted = 0;
    if (parse_ipv4(hint, &hinted_ip)) {
        for (size_t i = 0; i < count; ++i)
            if (direct_host(&interfaces[i], hinted_ip)) {
                if (ops->probe(ops->context, &interfaces[i], hinted_ip) == 0) targeted = 1;
            }
    }
    uint64_t hosts = 0;
    for (size_t i = 0; i < count; ++i) {
        uint32_t inverse = ~interfaces[i].mask;
        if (!interfaces[i].mask || (inverse & (inverse + 1))) {
            oversized = 1;
            continue;
        }
        if (inverse > 1) hosts += (uint64_t)inverse - 1;
    }
    if (hosts > NVR_NETWORK_MAX_HOSTS) oversized = 1;

    /* Serialize broad discovery. While waiting, reread the shared OS table;
     * another camera's scan may already have resolved this camera. */
    while (gate && atomic_flag_test_and_set(gate)) {
        if (cancelled(cancel)) { failure = "Busca por MAC cancelada."; goto done; }
        if (ops->now(ops->context) >= deadline) goto done;
        ops->pause(ops->context, 25);
        match = read_matches(ops, interfaces, count, neighbors, mac, 0, host, host_size);
        if (match > 0) { result = 0; goto done; }
        if (match < 0) { failure = "Nao foi possivel consultar a tabela de vizinhos da rede."; goto done; }
    }
    owns_gate = gate != NULL;
    match = read_matches(ops, interfaces, count, neighbors, mac, 1, host, host_size);
    if (match > 0) { result = 0; goto done; }
    if (match < 0) { failure = "Nao foi possivel consultar a tabela de vizinhos da rede."; goto done; }
    /* A stale matching table entry is also a targeted candidate on wide
     * subnets. Allow its directed verification without scanning that subnet. */
    if (oversized && !targeted) {
        size_t neighbor_count = 0;
        if (ops->neighbors(ops->context, interfaces, count, neighbors,
                           NVR_NETWORK_MAX_NEIGHBORS, &neighbor_count) < 0) {
            failure = "Nao foi possivel consultar a tabela de vizinhos da rede.";
            goto done;
        }
        for (size_t i = 0; i < neighbor_count; ++i)
            if (!memcmp(neighbors[i].mac, mac, 6) &&
                find_interface(interfaces, count, neighbors[i].index, neighbors[i].address)) targeted = 1;
    }
    if (oversized && !targeted) {
        failure = "Rede muito grande para busca automatica por MAC (limite: 1024 enderecos). Informe tambem o IP atual ou reserve DHCP por MAC no modem.";
        goto done;
    }
    size_t interface_index = 0;
    uint32_t cursor = (interfaces[0].address & interfaces[0].mask) + 1;
    while (ops->now(ops->context) < deadline) {
        if (cancelled(cancel)) { failure = "Busca por MAC cancelada."; goto done; }
        if (!oversized && interface_index < count) {
            unsigned batch = 0;
            while (interface_index < count && batch < 32) {
                NvrNetworkInterface *interface = &interfaces[interface_index];
                uint32_t broadcast = (interface->address & interface->mask) | ~interface->mask;
                if (cursor >= broadcast) {
                    ++interface_index;
                    if (interface_index < count)
                        cursor = (interfaces[interface_index].address & interfaces[interface_index].mask) + 1;
                    continue;
                }
                uint32_t address = cursor++;
                if (!direct_host(interface, address)) continue;
                if (ops->probe(ops->context, interface, address) == 0) sent = 1;
                ++batch;
                if (cancelled(cancel)) { failure = "Busca por MAC cancelada."; goto done; }
            }
        }
        ops->pause(ops->context, 40);
        match = read_matches(ops, interfaces, count, neighbors, mac, 0, host, host_size);
        if (match > 0) { result = 0; goto done; }
        if (match < 0) { failure = "Nao foi possivel consultar a tabela de vizinhos da rede."; goto done; }
    }
    if (oversized)
        failure = "MAC nao confirmado no IP informado. Rede muito grande para busca automatica (limite: 1024 enderecos); reserve DHCP por MAC no modem.";
    else if (!sent && !targeted)
        failure = "Nao foi possivel sondar a LAN; verifique as interfaces e permissoes de rede.";
done:
    if (owns_gate) atomic_flag_clear(gate);
    ops->finish(ops->context, interfaces, count);
    free(neighbors);
    if (result < 0) {
        host[0] = '\0';
        set_error(error, error_size, failure);
    }
    return result;
}

#if defined(_WIN32) || defined(__linux__)
static uint64_t native_now(void *context) {
    (void)context;
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000 + (uint64_t)time.tv_nsec / 1000000;
#endif
}

static void native_pause(void *context, unsigned milliseconds) {
    (void)context;
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec delay = {(time_t)(milliseconds / 1000), (long)(milliseconds % 1000) * 1000000};
    nanosleep(&delay, NULL);
#endif
}

static int add_interface(NvrNetworkInterface *interfaces, size_t capacity,
                         size_t *count, uint32_t address, uint32_t mask, unsigned index) {
    if (!address || !mask || (address >> 24) == 127 || (address >> 24) == 0 ||
        address >= UINT32_C(0xe0000000)) return 0;
    for (size_t i = 0; i < *count; ++i)
        if (interfaces[i].index == index && interfaces[i].mask == mask &&
            (interfaces[i].address & mask) == (address & mask)) return 0;
    if (*count >= capacity) return -1;
    interfaces[*count] = (NvrNetworkInterface){address, mask, index, (intptr_t)-1};
    ++*count;
    return 0;
}

static void native_finish(void *context, NvrNetworkInterface *interfaces, size_t count) {
    (void)context;
    for (size_t i = 0; i < count; ++i) if (interfaces[i].socket != (intptr_t)-1) {
#ifdef _WIN32
        closesocket((SOCKET)interfaces[i].socket);
#else
        close((int)interfaces[i].socket);
#endif
    }
}
#endif

#ifdef _WIN32
static int native_interfaces(void *context, NvrNetworkInterface *interfaces,
                              size_t capacity, size_t *count) {
    (void)context;
    ULONG bytes = 15000;
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG status = ERROR_BUFFER_OVERFLOW;
    for (unsigned attempt = 0; attempt < 3 && status == ERROR_BUFFER_OVERFLOW; ++attempt) {
        free(adapters);
        if (bytes > 1048576) return -1;
        adapters = malloc(bytes);
        if (!adapters) return -1;
        status = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                     GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &bytes);
    }
    if (status != NO_ERROR) { free(adapters); return -1; }
    int result = 0;
    *count = 0;
    for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp ||
            (adapter->IfType != IF_TYPE_ETHERNET_CSMACD && adapter->IfType != IF_TYPE_IEEE80211)) continue;
        MIB_IF_ROW2 row;
        memset(&row, 0, sizeof(row));
        row.InterfaceIndex = adapter->IfIndex;
        if (GetIfEntry2(&row) != NO_ERROR || !row.InterfaceAndOperStatusFlags.HardwareInterface ||
            row.InterfaceAndOperStatusFlags.FilterInterface ||
            row.InterfaceAndOperStatusFlags.NotMediaConnected) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS *ip = adapter->FirstUnicastAddress; ip; ip = ip->Next) {
            if (!ip->Address.lpSockaddr || ip->Address.lpSockaddr->sa_family != AF_INET ||
                ip->OnLinkPrefixLength > 32) continue;
            uint32_t mask = ip->OnLinkPrefixLength == 0 ? 0 : UINT32_MAX << (32 - ip->OnLinkPrefixLength);
            uint32_t address = ntohl(((struct sockaddr_in *)ip->Address.lpSockaddr)->sin_addr.s_addr);
            if (add_interface(interfaces, capacity, count, address, mask, adapter->IfIndex) < 0) result = -1;
        }
    }
    free(adapters);
    return result;
}

static int native_neighbors(void *context, const NvrNetworkInterface *interfaces,
                             size_t interface_count, NvrNetworkNeighbor *neighbors,
                             size_t capacity, size_t *count) {
    (void)context;
    MIB_IPNET_TABLE2 *table = NULL;
    *count = 0;
    ULONG status = GetIpNetTable2(AF_INET, &table);
    if (status == ERROR_NOT_FOUND) return 0;
    if (status != NO_ERROR) return -1;
    int result = 0;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        MIB_IPNET_ROW2 *row = &table->Table[i];
        if (row->Address.si_family != AF_INET || row->PhysicalAddressLength != 6) continue;
        int local = 0;
        for (size_t j = 0; j < interface_count; ++j) if (interfaces[j].index == row->InterfaceIndex) local = 1;
        if (!local) continue;
        if (*count >= capacity) { result = -1; break; }
        NvrNetworkNeighbor *entry = &neighbors[(*count)++];
        entry->address = ntohl(row->Address.Ipv4.sin_addr.s_addr);
        entry->index = row->InterfaceIndex;
        memcpy(entry->mac, row->PhysicalAddress, 6);
        entry->reachable = row->State == NlnsReachable && !row->IsUnreachable;
        entry->age = 0;
    }
    FreeMibTable(table);
    return result;
}

static int native_probe(void *context, NvrNetworkInterface *interface, uint32_t address) {
    (void)context;
    SOCKET descriptor = (SOCKET)interface->socket;
    if (descriptor == INVALID_SOCKET) {
        descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor == INVALID_SOCKET) return -1;
        u_long nonblocking = 1;
        DWORD index = htonl(interface->index);
        DWORD ttl = 1;
        struct sockaddr_in local = {0};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(interface->address);
        if (ioctlsocket(descriptor, FIONBIO, &nonblocking) != 0 ||
            setsockopt(descriptor, IPPROTO_IP, IP_UNICAST_IF, (const char *)&index, sizeof(index)) != 0 ||
            setsockopt(descriptor, IPPROTO_IP, IP_TTL, (const char *)&ttl, sizeof(ttl)) != 0 ||
            bind(descriptor, (struct sockaddr *)&local, sizeof(local)) != 0) {
            closesocket(descriptor);
            return -1;
        }
        interface->socket = (intptr_t)descriptor;
    }
    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(9);
    destination.sin_addr.s_addr = htonl(address);
    return sendto(descriptor, "", 0, 0, (struct sockaddr *)&destination, sizeof(destination)) >= 0 ? 0 : -1;
}
#elif defined(__linux__)
static int native_interfaces(void *context, NvrNetworkInterface *interfaces,
                              size_t capacity, size_t *count) {
    (void)context;
    struct ifaddrs *list = NULL;
    if (getifaddrs(&list) < 0) return -1;
    *count = 0;
    int result = 0;
    for (struct ifaddrs *item = list; item; item = item->ifa_next) {
        if (!item->ifa_addr || !item->ifa_netmask || item->ifa_addr->sa_family != AF_INET ||
            !(item->ifa_flags & IFF_UP) || !(item->ifa_flags & IFF_RUNNING) ||
            !(item->ifa_flags & IFF_BROADCAST) ||
            (item->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT | IFF_NOARP))) continue;
        int ethernet = 0;
        for (struct ifaddrs *link = list; link; link = link->ifa_next)
            if (link->ifa_addr && link->ifa_addr->sa_family == AF_PACKET &&
                !strcmp(link->ifa_name, item->ifa_name) &&
                ((struct sockaddr_ll *)link->ifa_addr)->sll_hatype == ARPHRD_ETHER) ethernet = 1;
        if (!ethernet) continue;
        /* Hardware-only interfaces deliberately exclude TAP, containers and
         * VPNs that also expose Ethernet-shaped virtual interfaces. */
        char device[512];
        struct stat info;
        int length = snprintf(device, sizeof(device), "/sys/class/net/%s/device", item->ifa_name);
        if (length < 0 || (size_t)length >= sizeof(device) || stat(device, &info) < 0) continue;
        unsigned index = if_nametoindex(item->ifa_name);
        if (!index) continue;
        uint32_t address = ntohl(((struct sockaddr_in *)item->ifa_addr)->sin_addr.s_addr);
        uint32_t mask = ntohl(((struct sockaddr_in *)item->ifa_netmask)->sin_addr.s_addr);
        if (add_interface(interfaces, capacity, count, address, mask, index) < 0) result = -1;
    }
    freeifaddrs(list);
    return result;
}

static int native_neighbors(void *context, const NvrNetworkInterface *interfaces,
                             size_t interface_count, NvrNetworkNeighbor *neighbors,
                             size_t capacity, size_t *count) {
    (void)context;
    *count = 0;
    int descriptor = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (descriptor < 0) return -1;
    struct {
        struct nlmsghdr header;
        struct ndmsg neighbor;
    } request = {0};
    request.header.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
    request.header.nlmsg_type = RTM_GETNEIGH;
    request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.header.nlmsg_seq = 1;
    request.neighbor.ndm_family = AF_INET;
    struct sockaddr_nl kernel = {0};
    kernel.nl_family = AF_NETLINK;
    if (sendto(descriptor, &request, request.header.nlmsg_len, 0,
               (struct sockaddr *)&kernel, sizeof(kernel)) < 0) { close(descriptor); return -1; }
    uint64_t deadline = native_now(NULL) + 200;
    int result = -1;
    while (native_now(NULL) < deadline) {
        struct pollfd item = {descriptor, POLLIN, 0};
        int ready = poll(&item, 1, 20);
        if (ready < 0 && errno != EINTR) break;
        if (ready <= 0) continue;
        union { struct nlmsghdr align; unsigned char bytes[32768]; } buffer;
        struct iovec payload = {buffer.bytes, sizeof(buffer.bytes)};
        struct msghdr message = {0};
        message.msg_iov = &payload;
        message.msg_iovlen = 1;
        ssize_t received = recvmsg(descriptor, &message, MSG_DONTWAIT);
        if (received < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        if (received <= 0 || (message.msg_flags & MSG_TRUNC)) break;
        int remaining = (int)received;
        for (struct nlmsghdr *header = (struct nlmsghdr *)buffer.bytes;
             NLMSG_OK(header, remaining); header = NLMSG_NEXT(header, remaining)) {
            if (header->nlmsg_seq != 1) continue;
            if (header->nlmsg_type == NLMSG_DONE) {
                result = (header->nlmsg_flags & NLM_F_DUMP_INTR) ? -1 : 0;
                goto done;
            }
            if (header->nlmsg_type == NLMSG_ERROR) goto done;
            if (header->nlmsg_type != RTM_NEWNEIGH ||
                header->nlmsg_len < NLMSG_LENGTH(sizeof(struct ndmsg))) continue;
            struct ndmsg *entry = NLMSG_DATA(header);
            if (entry->ndm_family != AF_INET || (entry->ndm_flags & NTF_PROXY)) continue;
            int local = 0;
            for (size_t i = 0; i < interface_count; ++i)
                if (interfaces[i].index == (unsigned)entry->ndm_ifindex) local = 1;
            if (!local) continue;
            uint32_t address = 0, age = UINT32_MAX;
            unsigned char mac[6] = {0};
            int got_address = 0, got_mac = 0;
            int attributes_size = (int)(header->nlmsg_len - NLMSG_LENGTH(sizeof(*entry)));
            for (struct rtattr *attribute = (struct rtattr *)((char *)entry + NLMSG_ALIGN(sizeof(*entry)));
                 RTA_OK(attribute, attributes_size); attribute = RTA_NEXT(attribute, attributes_size)) {
                if (attribute->rta_type == NDA_DST && RTA_PAYLOAD(attribute) == 4) {
                    memcpy(&address, RTA_DATA(attribute), 4);
                    address = ntohl(address);
                    got_address = 1;
                } else if (attribute->rta_type == NDA_LLADDR && RTA_PAYLOAD(attribute) == 6) {
                    memcpy(mac, RTA_DATA(attribute), 6);
                    got_mac = 1;
                } else if (attribute->rta_type == NDA_CACHEINFO &&
                           RTA_PAYLOAD(attribute) >= sizeof(struct nda_cacheinfo)) {
                    struct nda_cacheinfo cache;
                    memcpy(&cache, RTA_DATA(attribute), sizeof(cache));
                    age = cache.ndm_confirmed;
                }
            }
            if (!got_address || !got_mac) continue;
            if (*count >= capacity) goto done;
            NvrNetworkNeighbor *neighbor = &neighbors[(*count)++];
            neighbor->address = address;
            neighbor->index = (unsigned)entry->ndm_ifindex;
            memcpy(neighbor->mac, mac, 6);
            neighbor->reachable = (entry->ndm_state & NUD_REACHABLE) != 0;
            neighbor->age = age;
        }
    }
done:
    close(descriptor);
    return result;
}

static int native_probe(void *context, NvrNetworkInterface *interface, uint32_t address) {
    (void)context;
    int descriptor = (int)interface->socket;
    if (descriptor < 0) {
        descriptor = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
        if (descriptor < 0) return -1;
        struct sockaddr_in local = {0};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(interface->address);
        if (bind(descriptor, (struct sockaddr *)&local, sizeof(local)) < 0) {
            close(descriptor);
            return -1;
        }
        interface->socket = descriptor;
    }
    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(9);
    destination.sin_addr.s_addr = htonl(address);
    union { struct cmsghdr align; unsigned char bytes[CMSG_SPACE(sizeof(struct in_pktinfo))]; } control = {0};
    struct iovec payload = {(void *)"", 0};
    struct msghdr message = {0};
    message.msg_name = &destination;
    message.msg_namelen = sizeof(destination);
    message.msg_iov = &payload;
    message.msg_iovlen = 1;
    message.msg_control = control.bytes;
    message.msg_controllen = sizeof(control.bytes);
    struct cmsghdr *header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = IPPROTO_IP;
    header->cmsg_type = IP_PKTINFO;
    header->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
    struct in_pktinfo info = {0};
    info.ipi_ifindex = (int)interface->index;
    info.ipi_spec_dst.s_addr = htonl(interface->address);
    memcpy(CMSG_DATA(header), &info, sizeof(info));
    /* A previous destination's ICMP error can be delivered on this unconnected
     * socket. Clear it so that it cannot suppress the next host's ARP probe. */
    int pending_error = 0;
    socklen_t error_size = sizeof(pending_error);
    getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &pending_error, &error_size);
    ssize_t sent = sendmsg(descriptor, &message, MSG_DONTWAIT | MSG_DONTROUTE);
    if (sent < 0 && (errno == ECONNREFUSED || errno == EHOSTUNREACH || errno == ENETUNREACH))
        sent = sendmsg(descriptor, &message, MSG_DONTWAIT | MSG_DONTROUTE);
    return sent >= 0 ? 0 : -1;
}
#endif

int nvr_network_resolve_mac(const char *mac, const char *host_hint,
                            const atomic_int *cancel, char *host,
                            size_t host_size, char *error, size_t error_size) {
#if defined(_WIN32) || defined(__linux__)
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return set_error(error, error_size, "Nao foi possivel iniciar a rede do Windows.");
#endif
    NvrNetworkOps ops = {NULL, native_interfaces, native_neighbors, native_probe,
                          native_now, native_pause, native_finish};
    int result = resolve_with_ops(&ops, mac, host_hint, cancel, host, host_size,
                                  error, error_size, &scan_gate);
#ifdef _WIN32
    WSACleanup();
#endif
    return result;
#else
    (void)mac; (void)host_hint; (void)cancel; (void)host; (void)host_size;
    return set_error(error, error_size, "Busca por MAC disponivel no Linux e no Windows; use IP neste sistema.");
#endif
}

#ifdef NVR_NETWORK_TESTING
int nvr_network_test_resolve(const NvrNetworkOps *ops, const char *mac,
                             const char *hint, const atomic_int *cancel,
                             char *host, size_t host_size, char *error,
                             size_t error_size, atomic_flag *gate) {
    return resolve_with_ops(ops, mac, hint, cancel, host, host_size, error, error_size, gate);
}
#endif
