/*
 * test_srstd_bridge.c - 验证双库数据结构转换函数
 *
 * 测试 srstd_channel_to_pxview / srstd_sdi_to_pxview / srstd_packet_to_pxview
 * 三个转换函数,覆盖字段映射、类型转换(enum 值差异)、内存所有权。
 *
 * === 编译说明 ===
 *
 * 本测试通过 -include srstd_rename.h 编译,所以源码中的:
 *   struct sr_channel       → struct srstd_channel       (上游 6 字段)
 *   struct sr_dev_inst      → struct srstd_dev_inst      (上游 13 字段)
 *   struct sr_datafeed_packet → struct srstd_datafeed_packet (上游 2 字段)
 *   struct sr_datafeed_logic  → struct srstd_datafeed_logic  (上游 3 字段)
 *
 * SR_CHANNEL_LOGIC / SR_DF_LOGIC / SR_ST_ACTIVE 等大写 enum 常量 **不** 被重命名
 * (srstd_rename.h 只重命名 sr_* 小写前缀的符号),所以它们保持上游值:
 *   SR_CHANNEL_LOGIC=10000, SR_CHANNEL_ANALOG=10001
 *   SR_DF_LOGIC=10004, SR_DF_FRAME_BEGIN=10005, SR_DF_ANALOG=10007
 *   SR_ST_ACTIVE=10003, SR_ST_STOPPING=10004
 *
 * === 关键约束:PXView 指针必须当作 OPAQUE ===
 *
 * srstd_bridge.h 中的 struct sr_channel 等前向声明会被重命名为 struct srstd_channel,
 * 所以 _alloc 返回的指针在测试中类型为 struct srstd_channel *。但实际内存是 PXView 的
 * struct sr_channel(32 字段,布局完全不同:PXView 第一个字段是 uint16_t index,
 * 上游第一个字段是 void *sdi 指针)。
 *
 * 因此:**绝对不能直接解引用 _alloc 返回的指针**(如 px_ch->index),否则读取的是
 * 错误偏移的数据。必须只通过 _get_* accessor 函数读取字段,这些函数在 srstd_bridge.c
 * 中编译(不带 rename),按 PXView 真实布局访问。
 *
 * 上游侧结构(up_ch/up_sdi/up_pkt)可以正常构造和访问,因为它们就是上游类型。
 *
 * Include path 注意:
 *   CMakeLists.txt 中 test target 使用 BEFORE PRIVATE 强制 libsigrokstd/include
 *   和 libsigrokstd/src 排在全局 ./libsigrok (PXView fork) 之前,否则会引用
 *   PXView 的 struct sr_dev_inst 布局,与上游布局不匹配。
 */

#include "srstd.h"              /* 上游 libsigrok.h + srstd_rename.h */
#include "libsigrok-internal.h" /* 上游 sr_dev_inst 完整定义 */
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* srstd_bridge.h 中的 struct sr_channel 等前向声明会被重命名为 struct srstd_channel,
 * 但这只影响类型标签名(编译期),不影响 ABI(指针就是指针)。
 * _alloc 返回的指针是 PXView 结构,必须当 OPAQUE 处理。 */
#include "srstd_bridge.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg) do { \
    checks++; \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } else { \
        printf("OK: %s\n", msg); \
    } \
} while (0)

/* 释放上游 sr_channel(g_strdup 的 name + 结构体本身) */
static void free_upstream_channel(gpointer data)
{
    struct sr_channel *ch = (struct sr_channel *)data;
    if (ch) {
        g_free(ch->name);
        g_free(ch);
    }
}

/* === 测试 1: channel 转换 === */
static void test_channel_conversion(void)
{
    struct sr_channel *up_ch;  /* 上游,可直接访问 */
    struct sr_channel *px_ch;  /* OPAQUE — PXView 结构,只用 accessor */

    printf("\n=== Test 1: channel conversion ===\n");

    /* 构造上游 sr_channel(重命名后为 srstd_channel,6 字段) */
    up_ch = g_new0(struct sr_channel, 1);
    up_ch->index = 7;
    up_ch->type = SR_CHANNEL_LOGIC;   /* 上游值 10000 */
    up_ch->enabled = TRUE;
    up_ch->name = g_strdup("D0");

    /* 分配 PXView 结构(OPAQUE) */
    px_ch = srstd_bridge_pxview_channel_alloc();
    CHECK(px_ch != NULL, "channel alloc returned non-NULL");

    /* 转换:上游 → PXView */
    srstd_channel_to_pxview(up_ch, px_ch);

    /* 通过 accessor 验证字段 */
    CHECK(srstd_bridge_pxview_channel_get_index(px_ch) == 7,
          "channel index == 7");
    CHECK(srstd_bridge_pxview_channel_get_type(px_ch) == 10000,
          "channel type == SR_CHANNEL_LOGIC (10000, same in both libs)");
    CHECK(srstd_bridge_pxview_channel_get_enabled(px_ch) == TRUE,
          "channel enabled == TRUE");
    CHECK(srstd_bridge_pxview_channel_get_name(px_ch) != NULL,
          "channel name not NULL (deep copied)");
    CHECK(strcmp(srstd_bridge_pxview_channel_get_name(px_ch), "D0") == 0,
          "channel name == 'D0'");
    CHECK(srstd_bridge_pxview_channel_get_vdiv(px_ch) == 0,
          "channel vdiv == 0 (PXView-specific, not set by bridge)");
    CHECK(srstd_bridge_pxview_channel_get_vga_ptr(px_ch) == NULL,
          "channel vga_ptr == NULL (PXView-specific, not set by bridge)");

    /* Analog 通道类型映射:上游 10001 → PXView 10002 */
    up_ch->type = SR_CHANNEL_ANALOG;  /* 上游值 10001 */
    srstd_channel_to_pxview(up_ch, px_ch);
    CHECK(srstd_bridge_pxview_channel_get_type(px_ch) == 10002,
          "channel type == SR_CHANNEL_ANALOG (10002, mapped from upstream 10001)");

    /* NULL name 安全处理 */
    g_free(up_ch->name);
    up_ch->name = NULL;
    srstd_channel_to_pxview(up_ch, px_ch);
    CHECK(srstd_bridge_pxview_channel_get_name(px_ch) == NULL,
          "channel name == NULL when source name is NULL");

    /* 清理 */
    srstd_bridge_pxview_channel_free(px_ch);
    g_free(up_ch);

    printf("Test 1: channel conversion done (%d checks)\n", checks);
}

/* === 测试 2: sdi 转换(含递归 channels) === */
static void test_sdi_conversion(void)
{
    struct sr_dev_inst *up_sdi;  /* 上游 */
    struct sr_dev_inst *px_sdi;  /* OPAQUE — PXView */
    struct sr_channel *ch1, *ch2;

    printf("\n=== Test 2: sdi conversion ===\n");

    /* 构造上游 sr_dev_inst(重命名后为 srstd_dev_inst) */
    up_sdi = g_new0(struct sr_dev_inst, 1);
    up_sdi->vendor = g_strdup("Siglent");
    up_sdi->model = g_strdup("SDS1102");
    up_sdi->version = g_strdup("v2.1");
    up_sdi->serial_num = g_strdup("SN12345");
    up_sdi->connection_id = g_strdup("USB::1234");
    up_sdi->status = SR_ST_ACTIVE;  /* 上游值 10003 */
    up_sdi->inst_type = SR_INST_USB;

    /* 添加两个通道到上游 sdi */
    ch1 = g_new0(struct sr_channel, 1);
    ch1->index = 0;
    ch1->type = SR_CHANNEL_LOGIC;
    ch1->enabled = TRUE;
    ch1->name = g_strdup("CLK");
    up_sdi->channels = g_slist_append(up_sdi->channels, ch1);

    ch2 = g_new0(struct sr_channel, 1);
    ch2->index = 1;
    ch2->type = SR_CHANNEL_ANALOG;
    ch2->enabled = FALSE;
    ch2->name = g_strdup("A0");
    up_sdi->channels = g_slist_append(up_sdi->channels, ch2);

    /* 分配 PXView sdi(OPAQUE) */
    px_sdi = srstd_bridge_pxview_sdi_alloc();
    CHECK(px_sdi != NULL, "sdi alloc returned non-NULL");

    /* 转换 */
    srstd_sdi_to_pxview(up_sdi, px_sdi);

    /* 验证字符串字段(深拷贝) */
    CHECK(srstd_bridge_pxview_sdi_get_vendor(px_sdi) != NULL,
          "sdi vendor not NULL");
    CHECK(strcmp(srstd_bridge_pxview_sdi_get_vendor(px_sdi), "Siglent") == 0,
          "sdi vendor == 'Siglent'");
    CHECK(strcmp(srstd_bridge_pxview_sdi_get_model(px_sdi), "SDS1102") == 0,
          "sdi model == 'SDS1102'");

    /* 验证 channels 列表递归转换 */
    CHECK(srstd_bridge_pxview_sdi_get_channel_count(px_sdi) == 2,
          "sdi channel count == 2 (recursively converted)");

    /* 验证 status 映射:上游 SR_ST_ACTIVE=10003 → PXView SR_ST_ACTIVE=10004
     * (PXView 多了 SR_ST_INCOMPATIBLE=10003,导致 ACTIVE/STOPPING 偏移 +1) */
    CHECK(srstd_bridge_pxview_sdi_get_status(px_sdi) == 10004,
          "sdi status == SR_ST_ACTIVE (10004, mapped from upstream 10003)");

    /* 验证字符串独立性(修改上游不影响 PXView 拷贝) */
    g_free(up_sdi->vendor);
    up_sdi->vendor = g_strdup("CHANGED");
    CHECK(strcmp(srstd_bridge_pxview_sdi_get_vendor(px_sdi), "Siglent") == 0,
          "sdi vendor independent from source (deep copy verified)");

    /* 清理 PXView(sdi_free 会递归释放 channels 中的 channel) */
    srstd_bridge_pxview_sdi_free(px_sdi);

    /* 清理上游 */
    g_slist_free_full(up_sdi->channels, free_upstream_channel);
    up_sdi->channels = NULL;
    g_free(up_sdi->vendor);
    g_free(up_sdi->model);
    g_free(up_sdi->version);
    g_free(up_sdi->serial_num);
    g_free(up_sdi->connection_id);
    g_free(up_sdi);

    printf("Test 2: sdi conversion done\n");
}

/* === 测试 3: packet LOGIC 转换 === */
static void test_packet_logic_conversion(void)
{
    struct sr_datafeed_packet *up_pkt;  /* 上游 */
    struct sr_datafeed_packet *px_pkt;  /* OPAQUE — PXView */
    struct sr_datafeed_logic *up_logic;
    uint8_t test_data[16];

    printf("\n=== Test 3: packet LOGIC conversion ===\n");

    /* 构造测试数据 */
    memset(test_data, 0xAB, sizeof(test_data));

    /* 构造上游 sr_datafeed_logic(重命名后为 srstd_datafeed_logic,3 字段) */
    up_logic = g_new0(struct sr_datafeed_logic, 1);
    up_logic->length = 128;       /* 128 samples */
    up_logic->unitsize = 2;       /* 2 bytes per sample */
    up_logic->data = test_data;   /* 共享指针(不拷贝) */

    /* 构造上游 sr_datafeed_packet(2 字段) */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = SR_DF_LOGIC;   /* 上游值 10004 */
    up_pkt->payload = up_logic;

    /* 分配 PXView packet(OPAQUE) */
    px_pkt = srstd_bridge_pxview_packet_alloc();
    CHECK(px_pkt != NULL, "packet alloc returned non-NULL");

    /* 转换 */
    int ret = srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(ret == 0, "packet LOGIC conversion returned 0 (success)");

    /* 验证 packet type(LOGIC 两库值相同:10004) */
    CHECK(srstd_bridge_pxview_packet_get_type(px_pkt) == 10004,
          "packet type == SR_DF_LOGIC (10004, same in both libs)");

    /* 验证 logic payload 字段 */
    CHECK(srstd_bridge_pxview_packet_logic_get_length(px_pkt) == 128,
          "logic length == 128");
    CHECK(srstd_bridge_pxview_packet_logic_get_unitsize(px_pkt) == 2,
          "logic unitsize == 2");
    CHECK(srstd_bridge_pxview_packet_logic_get_data(px_pkt) == (const void *)test_data,
          "logic data pointer shared (== test_data, not copied)");
    CHECK(srstd_bridge_pxview_packet_logic_get_format(px_pkt) == 0,
          "logic format == LA_CROSS_DATA (0, PXView default)");

    /* 清理 PXView packet(_free 释放 logic 结构体,但不释放 data 指针) */
    srstd_bridge_pxview_packet_free(px_pkt);

    /* 清理上游 */
    g_free(up_logic);
    g_free(up_pkt);

    printf("Test 3: packet LOGIC conversion done\n");
}

/* === 测试 4: packet type 映射(覆盖 enum 值差异) === */
static void test_packet_type_mapping(void)
{
    struct sr_datafeed_packet *up_pkt;
    struct sr_datafeed_packet *px_pkt;

    printf("\n=== Test 4: packet type mapping (enum value differences) ===\n");

    /* SR_DF_HEADER: 上游 10000 → PXView 10000 (相同) */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = SR_DF_HEADER;
    px_pkt = srstd_bridge_pxview_packet_alloc();
    srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(srstd_bridge_pxview_packet_get_type(px_pkt) == 10000,
          "HEADER: upstream 10000 → PXView 10000 (same)");
    CHECK(srstd_bridge_pxview_packet_logic_get_length(px_pkt) == 0,
          "HEADER: no logic payload (length==0)");
    srstd_bridge_pxview_packet_free(px_pkt);
    g_free(up_pkt);

    /* SR_DF_FRAME_BEGIN: 上游 10005 → PXView 10007 (差 +2) */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = SR_DF_FRAME_BEGIN;
    px_pkt = srstd_bridge_pxview_packet_alloc();
    srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(srstd_bridge_pxview_packet_get_type(px_pkt) == 10007,
          "FRAME_BEGIN: upstream 10005 → PXView 10007 (+2, PXView has DSO=10005)");
    srstd_bridge_pxview_packet_free(px_pkt);
    g_free(up_pkt);

    /* SR_DF_FRAME_END: 上游 10006 → PXView 10008 */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = SR_DF_FRAME_END;
    px_pkt = srstd_bridge_pxview_packet_alloc();
    srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(srstd_bridge_pxview_packet_get_type(px_pkt) == 10008,
          "FRAME_END: upstream 10006 → PXView 10008");
    srstd_bridge_pxview_packet_free(px_pkt);
    g_free(up_pkt);

    /* SR_DF_ANALOG: 上游 10007 → PXView 10006 (反向偏移) */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = SR_DF_ANALOG;
    px_pkt = srstd_bridge_pxview_packet_alloc();
    srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(srstd_bridge_pxview_packet_get_type(px_pkt) == 10006,
          "ANALOG: upstream 10007 → PXView 10006 (PXView ANALOG before FRAME_BEGIN)");
    srstd_bridge_pxview_packet_free(px_pkt);
    g_free(up_pkt);

    /* SR_DF_END: 上游 10001 → PXView 10001 (相同) */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = SR_DF_END;
    px_pkt = srstd_bridge_pxview_packet_alloc();
    srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(srstd_bridge_pxview_packet_get_type(px_pkt) == 10001,
          "END: upstream 10001 → PXView 10001 (same)");
    srstd_bridge_pxview_packet_free(px_pkt);
    g_free(up_pkt);

    /* 无效类型 → 返回错误 */
    up_pkt = g_new0(struct sr_datafeed_packet, 1);
    up_pkt->type = 99999;
    px_pkt = srstd_bridge_pxview_packet_alloc();
    int ret = srstd_packet_to_pxview(up_pkt, px_pkt);
    CHECK(ret != 0, "invalid packet type (99999) returns error (ret != 0)");
    srstd_bridge_pxview_packet_free(px_pkt);
    g_free(up_pkt);

    /* NULL 参数安全处理 */
    ret = srstd_packet_to_pxview(NULL, NULL);
    CHECK(ret != 0, "NULL src/dst returns error");

    printf("Test 4: packet type mapping done\n");
}

/* === 测试 5: device status 映射 === */
static void test_status_mapping(void)
{
    struct sr_dev_inst *up_sdi;
    struct sr_dev_inst *px_sdi;

    printf("\n=== Test 5: device status mapping ===\n");

    up_sdi = g_new0(struct sr_dev_inst, 1);

    /* SR_ST_NOT_FOUND: 10000 → 10000 (相同) */
    up_sdi->status = SR_ST_NOT_FOUND;
    px_sdi = srstd_bridge_pxview_sdi_alloc();
    srstd_sdi_to_pxview(up_sdi, px_sdi);
    CHECK(srstd_bridge_pxview_sdi_get_status(px_sdi) == 10000,
          "NOT_FOUND: 10000 → 10000 (same)");
    srstd_bridge_pxview_sdi_free(px_sdi);

    /* SR_ST_ACTIVE: 上游 10003 → PXView 10004 */
    up_sdi->status = SR_ST_ACTIVE;
    px_sdi = srstd_bridge_pxview_sdi_alloc();
    srstd_sdi_to_pxview(up_sdi, px_sdi);
    CHECK(srstd_bridge_pxview_sdi_get_status(px_sdi) == 10004,
          "ACTIVE: upstream 10003 → PXView 10004 (+1, PXView has INCOMPATIBLE=10003)");
    srstd_bridge_pxview_sdi_free(px_sdi);

    /* SR_ST_STOPPING: 上游 10004 → PXView 10005 */
    up_sdi->status = SR_ST_STOPPING;
    px_sdi = srstd_bridge_pxview_sdi_alloc();
    srstd_sdi_to_pxview(up_sdi, px_sdi);
    CHECK(srstd_bridge_pxview_sdi_get_status(px_sdi) == 10005,
          "STOPPING: upstream 10004 → PXView 10005");
    srstd_bridge_pxview_sdi_free(px_sdi);

    g_free(up_sdi);

    printf("Test 5: device status mapping done\n");
}

/* === 测试 6: size 函数 === */
static void test_size_functions(void)
{
    printf("\n=== Test 6: size functions ===\n");

    CHECK(srstd_bridge_pxview_channel_size() > 0,
          "channel size > 0 (PXView sr_channel is 32 fields, large)");
    CHECK(srstd_bridge_pxview_sdi_size() > 0,
          "sdi size > 0 (PXView sr_dev_inst is 19 fields)");
    CHECK(srstd_bridge_pxview_packet_size() > 0,
          "packet size > 0 (PXView sr_datafeed_packet is 4 fields)");

    /* PXView channel 比 上游 channel 大很多(32 字段 vs 6 字段) */
    CHECK(srstd_bridge_pxview_channel_size() >= sizeof(void *) * 4,
          "channel size >= 4 pointers (PXView has 32 fields, much larger than upstream 6)");

    printf("Test 6: size functions done\n");
}

int main(void)
{
    /* 无缓冲输出,崩溃时也能看到已打印的信息 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("=== srstd_bridge conversion tests ===\n");
    printf("PXView struct sizes: channel=%zu, sdi=%zu, packet=%zu\n",
           srstd_bridge_pxview_channel_size(),
           srstd_bridge_pxview_sdi_size(),
           srstd_bridge_pxview_packet_size());

    test_channel_conversion();
    test_sdi_conversion();
    test_packet_logic_conversion();
    test_packet_type_mapping();
    test_status_mapping();
    test_size_functions();

    printf("\n=== Summary: %d checks, %d failures ===\n", checks, failures);
    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\nTESTS FAILED\n");
    return 1;
}
