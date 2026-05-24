/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2021 original Python version
 * Copyright (C) 2024 C port
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

/* Annotations */
enum {
    ANN_HEADER = 0,
    ANN_DATA,
    NUM_ANN,
};

/* Decoder private state */
typedef struct {
    int out_ann;
    int out_python;
    uint64_t ss_block;
    uint64_t es_block;
} ipv4_state;

/* --- IP Protocol lookup table --- */

typedef struct {
    uint8_t protocol;
    const char *short_name;
    const char *long_name;
} ip_protocol_entry;

static const ip_protocol_entry ip_protocol_table[] = {
    {0x00, "HOPOPT", "IPv6 Hop-by-Hop Option"},
    {0x01, "ICMP", "Internet Control Message Protocol"},
    {0x02, "IGMP", "Internet Group Management Protocol"},
    {0x03, "GGP", "Gateway-to-Gateway Protocol"},
    {0x04, "IP-in-IP", "IP in IP (encapsulation)"},
    {0x05, "ST", "Internet Stream Protocol"},
    {0x06, "TCP", "Transmission Control Protocol"},
    {0x07, "CBT", "Core-based trees"},
    {0x08, "EGP", "Exterior Gateway Protocol"},
    {0x09, "IGP", "Interior Gateway Protocol"},
    {0x0A, "BBN-RCC-MON", "BBN RCC Monitoring"},
    {0x0B, "NVP-II", "Network Voice Protocol"},
    {0x0C, "PUP", "Xerox PUP"},
    {0x0D, "ARGUS", "ARGUS"},
    {0x0E, "EMCON", "EMCON"},
    {0x0F, "XNET", "Cross Net Debugger"},
    {0x10, "CHAOS", "Chaos"},
    {0x11, "UDP", "User Datagram Protocol"},
    {0x12, "MUX", "Multiplexing"},
    {0x13, "DCN-MEAS", "DCN Measurement Subsystems"},
    {0x14, "HMP", "Host Monitoring Protocol"},
    {0x15, "PRM", "Packet Radio Measurement"},
    {0x16, "XNS-IDP", "XEROX NS IDP"},
    {0x17, "TRUNK-1", "Trunk-1"},
    {0x18, "TRUNK-2", "Trunk-2"},
    {0x19, "LEAF-1", "Leaf-1"},
    {0x1A, "LEAF-2", "Leaf-2"},
    {0x1B, "RDP", "Reliable Data Protocol"},
    {0x1C, "IRTP", "Internet Reliable Transaction Protocol"},
    {0x1D, "ISO-TP4", "ISO Transport Protocol Class 4"},
    {0x1E, "NETBLT", "Bulk Data Transfer Protocol"},
    {0x1F, "MFE-NSP", "MFE Network Services Protocol"},
    {0x20, "MERIT-INP", "MERIT Internodal Protocol"},
    {0x21, "DCCP", "Datagram Congestion Control Protocol"},
    {0x22, "3PC", "Third Party Connect Protocol"},
    {0x23, "IDPR", "Inter-Domain Policy Routing Protocol"},
    {0x24, "XTP", "Xpress Transport Protocol"},
    {0x25, "DDP", "Datagram Delivery Protocol"},
    {0x26, "IDPR-CMTP", "IDPR Control Message Transport Protocol"},
    {0x27, "TP++", "TP++ Transport Protocol"},
    {0x28, "IL", "IL Transport Protocol"},
    {0x29, "IPv6", "IPv6 Encapsulation"},
    {0x2A, "SDRP", "Source Demand Routing Protocol"},
    {0x2B, "IPv6-Route", "Routing Header for IPv6"},
    {0x2C, "IPv6-Frag", "Fragment Header for IPv6"},
    {0x2D, "IDRP", "Inter-Domain Routing Protocol"},
    {0x2E, "RSVP", "Resource Reservation Protocol"},
    {0x2F, "GRE", "Generic Routing Encapsulation"},
    {0x30, "DSR", "Dynamic Source Routing Protocol"},
    {0x31, "BNA", "Burroughs Network Architecture"},
    {0x32, "ESP", "Encapsulating Security Payload"},
    {0x33, "AH", "Authentication Header"},
    {0x34, "I-NLSP", "Integrated Net Layer Security Protocol"},
    {0x35, "SwIPe", "SwIPe"},
    {0x36, "NARP", "NBMA Address Resolution Protocol"},
    {0x37, "MOBILE", "IP Mobility (Min Encap)"},
    {0x38, "TLSP", "Transport Layer Security Protocol"},
    {0x39, "SKIP", "Simple Key-Management for Internet Protocol"},
    {0x3A, "IPv6-ICMP", "ICMP for IPv6"},
    {0x3B, "IPv6-NoNxt", "No Next Header for IPv6"},
    {0x3C, "IPv6-Opts", "Destination Options for IPv6"},
    {0x3E, "CFTP", "CFTP"},
    {0x40, "SAT-EXPAK", "SATNET and Backroom EXPAK"},
    {0x41, "KRYPTOLAN", "Kryptolan"},
    {0x42, "RVD", "MIT Remote Virtual Disk Protocol"},
    {0x43, "IPPC", "Internet Pluribus Packet Core"},
    {0x45, "SAT-MON", "SATNET Monitoring"},
    {0x46, "VISA", "VISA Protocol"},
    {0x47, "IPCU", "Internet Packet Core Utility"},
    {0x48, "CPNX", "Computer Protocol Network Executive"},
    {0x49, "CPHB", "Computer Protocol Heart Beat"},
    {0x4A, "WSN", "Wang Span Network"},
    {0x4B, "PVP", "Packet Video Protocol"},
    {0x4C, "BR-SAT-MON", "Backroom SATNET Monitoring"},
    {0x4D, "SUN-ND", "SUN ND PROTOCOL-Temporary"},
    {0x4E, "WB-MON", "WIDEBAND Monitoring"},
    {0x4F, "WB-EXPAK", "WIDEBAND EXPAK"},
    {0x50, "ISO-IP", "ISO Internet Protocol"},
    {0x51, "VMTP", "Versatile Message Transaction Protocol"},
    {0x52, "SECURE-VMTP", "Secure VMTP"},
    {0x53, "VINES", "VINES"},
    {0x54, "TTP", "TTP"},
    {0x55, "NSFNET-IGP", "NSFNET-IGP"},
    {0x56, "DGP", "Dissimilar Gateway Protocol"},
    {0x57, "TCF", "TCF"},
    {0x58, "EIGRP", "EIGRP"},
    {0x59, "OSPF", "Open Shortest Path First"},
    {0x5A, "Sprite-RPC", "Sprite RPC Protocol"},
    {0x5B, "LARP", "Locus Address Resolution Protocol"},
    {0x5C, "MTP", "Multicast Transport Protocol"},
    {0x5D, "AX.25", "AX.25"},
    {0x5E, "OS", "KA9Q NOS compatible IP over IP tunneling"},
    {0x5F, "MICP", "Mobile Internetworking Control Protocol"},
    {0x60, "SCC-SP", "Semaphore Communications Sec. Pro"},
    {0x61, "ETHERIP", "Ethernet-within-IP Encapsulation"},
    {0x62, "ENCAP", "Encapsulation Header"},
    {0x64, "GMTP", "GMTP"},
    {0x65, "IFMP", "Ipsilon Flow Management Protocol"},
    {0x66, "PNNI", "PNNI over IP"},
    {0x67, "PIM", "Protocol Independent Multicast"},
    {0x68, "ARIS", "IBM's ARIS Protocol"},
    {0x69, "SCPS", "SCPS"},
    {0x6A, "QNX", "QNX"},
    {0x6B, "A/N", "Active Networks"},
    {0x6C, "IPComp", "IP Payload Compression Protocol"},
    {0x6D, "SNP", "Sitara Networks Protocol"},
    {0x6E, "Compaq-Peer", "Compaq Peer Protocol"},
    {0x6F, "IPX-in-IP", "IPX in IP"},
    {0x70, "VRRP", "Virtual Router Redundancy Protocol"},
    {0x71, "PGM", "PGM Reliable Transport Protocol"},
    {0x73, "L2TP", "Layer Two Tunneling Protocol Version 3"},
    {0x74, "DDX", "D-II Data Exchange"},
    {0x75, "IATP", "Interactive Agent Transfer Protocol"},
    {0x76, "STP", "Schedule Transfer Protocol"},
    {0x77, "SRP", "SpectraLink Radio Protocol"},
    {0x78, "UTI", "Universal Transport Interface Protocol"},
    {0x79, "SMP", "Simple Message Protocol"},
    {0x7A, "SM", "Simple Multicast Protocol"},
    {0x7B, "PTP", "Performance Transparency Protocol"},
    {0x7C, "IS-IS over IPv4", "IS-IS Protocol over IPv4"},
    {0x7D, "FIRE", "Flexible Intra-AS Routing Environment"},
    {0x7E, "CRTP", "Combat Radio Transport Protocol"},
    {0x7F, "CRUDP", "Combat Radio User Datagram"},
    {0x80, "SSCOPMCE", "Service-Specific Connection-Oriented Protocol"},
    {0x82, "SPS", "Secure Packet Shield"},
    {0x83, "PIPE", "Private IP Encapsulation within IP"},
    {0x84, "SCTP", "Stream Control Transmission Protocol"},
    {0x85, "FC", "Fibre Channel"},
    {0x86, "RSVP-E2E-IGNORE", "Reservation Protocol End-to-End Ignore"},
    {0x87, "Mobility Header", "Mobility Extension Header for IPv6"},
    {0x88, "UDPLite", "Lightweight User Datagram Protocol"},
    {0x89, "MPLS-in-IP", "MPLS Encapsulated in IP"},
    {0x8A, "manet", "MANET Protocols"},
    {0x8B, "HIP", "Host Identity Protocol"},
    {0x8C, "Shim6", "Site Multihoming by IPv6 Intermediation"},
    {0x8D, "WESP", "Wrapped Encapsulating Security Payload"},
    {0x8E, "ROHC", "Robust Header Compression"},
};
#define IP_PROTOCOL_COUNT (sizeof(ip_protocol_table) / sizeof(ip_protocol_table[0]))

static const ip_protocol_entry *find_ip_protocol(uint8_t proto)
{
    for (int i = 0; i < (int)IP_PROTOCOL_COUNT; i++)
        if (ip_protocol_table[i].protocol == proto) return &ip_protocol_table[i];
    return NULL;
}

/* --- Helper to read block ss/es --- */

static uint64_t blk_ss(const uint8_t *blocks_data, int idx)
{
    uint64_t v;
    memcpy(&v, blocks_data + idx * 16, 8);
    return v;
}

static uint64_t blk_es(const uint8_t *blocks_data, int idx)
{
    uint64_t v;
    memcpy(&v, blocks_data + idx * 16 + 8, 8);
    return v;
}

/* --- IP Header Checksum --- */

static int ip_checksum_ok(const uint8_t *header, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len; i += 2)
        sum += ((uint32_t)header[i] << 8) | header[i + 1];
    sum = (sum + (sum >> 16)) & 0xFFFF;
    return (sum == 0xFFFF) ? 1 : 0;
}

/* --- Decoder metadata --- */

static const char *ipv4_inputs[] = {"ethernet", NULL};
static const char *ipv4_outputs[] = {"ipv4", NULL};
static const char *ipv4_tags[] = {"Networking", "PC", NULL};

static const char *ipv4_ann_labels[][3] = {
    {"", "header", "Decoded header"},
    {"", "data", "Decoded data"},
};

static const int row_headers_classes[] = {ANN_HEADER, -1};
static const int row_datas_classes[] = {ANN_DATA, -1};

static const struct srd_c_ann_row ipv4_ann_rows[] = {
    {"headers", "Headers", row_headers_classes, 1},
    {"datas", "Datas", row_datas_classes, 1},
};

/* --- Core recv_proto --- */

static void ipv4_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ipv4_state *s = (ipv4_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "PAYLOAD") != 0 || !data || data_len < 4)
        return;

    /* Parse PAYLOAD data layout */
    uint16_t payload_len;
    memcpy(&payload_len, data, 2);
    const uint8_t *payload = data + 2;

    uint16_t block_count;
    memcpy(&block_count, data + 2 + payload_len, 2);
    const uint8_t *blocks_data = data + 4 + payload_len;

    /* Get IHL */
    int ihl = (payload[0] & 0x0F) * 4;
    if (ihl != 20) return; /* Only support standard 20-byte header */

    if ((int)payload_len < 20 || (int)block_count < 20)
        return;

    char t[128], t2[64], t3[16];

    /* Version + Header Length */
    s->ss_block = blk_ss(blocks_data, 0);
    s->es_block = blk_es(blocks_data, 0);
    snprintf(t, sizeof(t), "Version: 4 Header Length: %d bytes", ihl);
    snprintf(t2, sizeof(t2), "Version: 4 Len: %d", ihl);
    snprintf(t3, sizeof(t3), "4/%d", ihl);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2, t3);

    /* DSCP and ECN */
    s->ss_block = blk_ss(blocks_data, 1);
    s->es_block = blk_es(blocks_data, 1);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER,
              "Differentiated Services Code Point (DSCP) and Explicit Congestion Notification (ECN)",
              "DSCP and ECN");

    /* Total packet length */
    uint16_t total_length = ((uint16_t)payload[2] << 8) | payload[3];
    s->ss_block = blk_ss(blocks_data, 2);
    s->es_block = blk_es(blocks_data, 3);
    snprintf(t, sizeof(t), "Packet Length: %d bytes", total_length);
    snprintf(t2, sizeof(t2), "Pkt Len: %d", total_length);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* Identification */
    uint16_t ident = ((uint16_t)payload[4] << 8) | payload[5];
    s->ss_block = blk_ss(blocks_data, 4);
    s->es_block = blk_es(blocks_data, 5);
    snprintf(t, sizeof(t), "Identification: %d", ident);
    snprintf(t2, sizeof(t2), "ID: %d", ident);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* Flags */
    int df = (payload[6] & 0x40) >> 6;
    int mf = (payload[6] & 0x20) >> 5;
    s->ss_block = blk_ss(blocks_data, 6);
    uint64_t flags_es = blk_ss(blocks_data, 6) + (uint64_t)(((blk_es(blocks_data, 6) - blk_ss(blocks_data, 6)) * 3) / 8);
    snprintf(t, sizeof(t), "Don't Fragment: %d    More Fragments: %d", df, mf);
    snprintf(t2, sizeof(t2), "DF: %d    MF: %d", df, mf);
    C_ANN_PUT(di, s->ss_block, flags_es, s->out_ann, ANN_HEADER, t, t2, "DF and MF", "Flags");

    /* Fragment offset */
    uint16_t frag_offset = (((uint16_t)(payload[6] & 0x1F) << 8) | payload[7]) * 8;
    s->ss_block = flags_es;
    s->es_block = blk_es(blocks_data, 7);
    snprintf(t, sizeof(t), "Fragment Offset: %d bytes", frag_offset);
    snprintf(t2, sizeof(t2), "Offset: %d", frag_offset);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* TTL */
    uint8_t ttl = payload[8];
    s->ss_block = blk_ss(blocks_data, 8);
    s->es_block = blk_es(blocks_data, 8);
    snprintf(t, sizeof(t), "Time To Live: %d", ttl);
    snprintf(t2, sizeof(t2), "TTL: %d", ttl);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* Protocol */
    uint8_t protocol = payload[9];
    s->ss_block = blk_ss(blocks_data, 9);
    s->es_block = blk_es(blocks_data, 9);
    const ip_protocol_entry *proto_entry = find_ip_protocol(protocol);
    if (proto_entry) {
        snprintf(t, sizeof(t), "Protocol: %s (%d)", proto_entry->long_name, proto_entry->short_name[0] ? 0 : protocol);
        snprintf(t, sizeof(t), "Protocol: %s (%s)", proto_entry->long_name, proto_entry->short_name);
        snprintf(t2, sizeof(t2), "%s", proto_entry->short_name);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);
    } else {
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, "Protocol: UNKNOWN", "UNKNOWN");
    }

    /* Header checksum */
    int cs_ok = ip_checksum_ok(payload, ihl);
    const char *cs_str = cs_ok ? "OK" : "FAILED";
    s->ss_block = blk_ss(blocks_data, 10);
    s->es_block = blk_es(blocks_data, 11);
    snprintf(t, sizeof(t), "Header Checksum: %s", cs_str);
    snprintf(t2, sizeof(t2), "Checksum: %s", cs_str);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* Source IP */
    char src_ip[16];
    snprintf(src_ip, sizeof(src_ip), "%d.%d.%d.%d", payload[12], payload[13], payload[14], payload[15]);
    s->ss_block = blk_ss(blocks_data, 12);
    s->es_block = blk_es(blocks_data, 15);
    snprintf(t, sizeof(t), "Source IP Address: %s", src_ip);
    snprintf(t2, sizeof(t2), "Src IP: %s", src_ip);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* Destination IP */
    char dst_ip[16];
    snprintf(dst_ip, sizeof(dst_ip), "%d.%d.%d.%d", payload[16], payload[17], payload[18], payload[19]);
    s->ss_block = blk_ss(blocks_data, 16);
    s->es_block = blk_es(blocks_data, 19);
    snprintf(t, sizeof(t), "Destination IP Address: %s", dst_ip);
    snprintf(t2, sizeof(t2), "DEST IP: %s", dst_ip);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t, t2);

    /* IP Payload annotations */
    for (int i = 20; i < (int)payload_len && i < (int)block_count; i++) {
        s->ss_block = blk_ss(blocks_data, i);
        s->es_block = blk_es(blocks_data, i);
        snprintf(t, sizeof(t), "0x%02X", payload[i]);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_DATA, t);
    }

    /* Push payload to stacked decoders */
    if (s->out_python >= 0 && (int)payload_len > 20 && (int)block_count > 20) {
        uint16_t ip_plen = (uint16_t)(payload_len - 20);
        uint16_t bc = (uint16_t)(block_count - 20);
        int buf_size = 2 + ip_plen + 2 + bc * 16 + 4 + 4;
        uint8_t *buf = g_malloc(buf_size);
        int pos = 0;
        memcpy(buf + pos, &ip_plen, 2); pos += 2;
        memcpy(buf + pos, payload + 20, ip_plen); pos += ip_plen;
        memcpy(buf + pos, &bc, 2); pos += 2;
        memcpy(buf + pos, blocks_data + 20 * 16, bc * 16); pos += bc * 16;
        memcpy(buf + pos, payload + 12, 4); pos += 4;  /* src_ip */
        memcpy(buf + pos, payload + 16, 4); pos += 4;  /* dst_ip */
        c_decoder_put_python(di, blk_ss(blocks_data, 20), blk_es(blocks_data, block_count - 1),
                             s->out_python, "IP_PAYLOAD", buf, pos);
        g_free(buf);
    }
}

/* --- Decoder lifecycle --- */

static void ipv4_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(ipv4_state)));
    ipv4_state *s = (ipv4_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(ipv4_state));
}

static void ipv4_start(struct srd_decoder_inst *di)
{
    ipv4_state *s = (ipv4_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "ipv4");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PROTO, "ipv4");
}

static void ipv4_decode(struct srd_decoder_inst *di)
{
    (void)di;
}

static void ipv4_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder ipv4_c_decoder = {
    .id = "ipv4_c",
    .name = "IPv4(C)",
    .longname = "Internet Protocol Version 4 (C)",
    .desc = "IPv4 (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = ipv4_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = ipv4_ann_rows,
    .inputs = ipv4_inputs,
    .num_inputs = 1,
    .outputs = ipv4_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = ipv4_tags,
    .num_tags = 2,
    .reset = ipv4_reset,
    .start = ipv4_start,
    .decode = ipv4_decode,
    .destroy = ipv4_destroy,
    .recv_proto = ipv4_recv_proto,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &ipv4_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
