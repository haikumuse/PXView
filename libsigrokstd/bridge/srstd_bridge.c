/*
 * srstd_bridge.c - 双库数据结构转换实现
 *
 * 把上游 libsigrokstd 的 srstd_dev_inst/srstd_channel/srstd_datafeed_packet
 * 转换为 PXView libsigrok 的对应结构。
 *
 * === 编译说明 ===
 *
 * 本文件必须 **不** 通过 -include srstd_rename.h 编译。原因:
 *   - 本文件 include PXView 的 libsigrok.h / libsigrok-internal.h,其中定义了
 *     SR_CHANNEL_LOGIC=10000 / SR_DF_ANALOG=10006 等 PXView enum 常量。
 *   - 如果注入 srstd_rename.h,struct sr_channel 等标签会被重命名为 srstd_channel,
 *     导致与 PXView 头文件中的 struct sr_channel 定义冲突(类型不匹配)。
 *   - 同时 srstd_rename.h 不重命名 enum 常量(SR_DF_* 大写),但本文件需要 PXView
 *     的 enum 值,所以不能用上游头文件。
 *
 * 在 CMakeLists.txt 中,本文件通过单独的 OBJECT 库(libsigrokstd_bridge)编译,
 * 该库不注入 srstd_rename.h,也不带 libsigrokstd 的 BEFORE include 路径,
 * 所以 #include "libsigrok.h" 解析到 PXView 的 ./libsigrok/libsigrok.h。
 *
 * === 影子结构模式 ===
 *
 * 上游结构布局通过本文件内定义的 srstd_bridge_*_shadow 影子结构匹配。
 * 影子结构使用与上游完全相同的字段顺序和类型,但使用自己的 struct 标签名
 * (避免与 PXView 的 struct sr_channel 等冲突)。转换时把 void* 强转为
 * shadow 指针,按偏移读取上游字段。
 */

/* PXView libsigrok 头文件(非上游)。
 * 全局 include 路径 ./libsigrok 使这些 include 解析到 PXView 版本。 */
#include "libsigrok.h"
#include "libsigrok-internal.h" /* struct sr_dev_inst 完整定义 */

#include <glib.h>
#include <string.h>

#include "srstd_bridge.h"

/* === 上游 enum 常量值(定义为宏,避免 include 上游 libsigrok.h) ===
 * 值来自 libsigrokstd/include/libsigrok/libsigrok.h。
 * 不用上游 enum 是因为同一 TU 内不能同时有两个 libsigrok.h(enum 常量重定义)。 */

/* enum sr_channeltype (上游) */
#define UP_SR_CHANNEL_LOGIC   10000
#define UP_SR_CHANNEL_ANALOG  10001

/* enum sr_packettype (上游) */
#define UP_SR_DF_HEADER       10000
#define UP_SR_DF_END          10001
#define UP_SR_DF_META         10002
#define UP_SR_DF_TRIGGER      10003
#define UP_SR_DF_LOGIC        10004
#define UP_SR_DF_FRAME_BEGIN  10005
#define UP_SR_DF_FRAME_END    10006
#define UP_SR_DF_ANALOG       10007

/* enum sr_device_status (上游,无 SR_ST_INCOMPATIBLE) */
#define UP_SR_ST_NOT_FOUND      10000
#define UP_SR_ST_INITIALIZING   10001
#define UP_SR_ST_INACTIVE       10002
#define UP_SR_ST_ACTIVE         10003
#define UP_SR_ST_STOPPING       10004

/* enum sr_trigger_matches (上游,值来自 libsigrokstd/include/libsigrok/libsigrok.h) */
#define UP_SR_TRIGGER_ZERO      1
#define UP_SR_TRIGGER_ONE       2
#define UP_SR_TRIGGER_RISING    3
#define UP_SR_TRIGGER_FALLING   4
#define UP_SR_TRIGGER_EDGE      5
#define UP_SR_TRIGGER_OVER      6
#define UP_SR_TRIGGER_UNDER     7

/* === 影子结构:匹配上游布局 ===
 * 字段顺序/类型与上游 struct sr_channel 等完全一致(参见 srstd_bridge.h 顶部注释)。
 * 使用自己的标签名避免与 PXView struct 冲突。 */

struct srstd_bridge_channel_shadow {
    void *sdi;        /* struct sr_dev_inst * (上游) */
    int index;
    int type;
    gboolean enabled;
    char *name;
    void *priv;
};

struct srstd_bridge_sdi_shadow {
    void *driver;     /* struct sr_dev_driver * */
    int status;
    int inst_type;
    char *vendor;
    char *model;
    char *version;
    char *serial_num;
    char *connection_id;
    GSList *channels;
    GSList *channel_groups;
    void *conn;
    void *priv;
    void *session;    /* struct sr_session * */
};

struct srstd_bridge_packet_shadow {
    uint16_t type;
    const void *payload;
};

struct srstd_bridge_logic_shadow {
    uint64_t length;
    uint16_t unitsize;
    void *data;
};

struct srstd_bridge_analog_shadow {
    void *data;
    uint32_t num_samples;
    void *encoding;
    void *meaning;
    void *spec;
};

/* 上游 sr_trigger 系列影子结构(匹配 libsigrokstd/include/libsigrok/libsigrok.h 布局)。
 * 字段顺序/类型与上游完全一致;channel 用 void* 表达"opaque 指针"
 * (上游 soft_trigger_logic_check 会解引用 channel->index/enabled,见已知限制)。 */
struct srstd_bridge_trigger_shadow {
    char *name;
    GSList *stages;
};

struct srstd_bridge_trigger_stage_shadow {
    int stage;
    GSList *matches;
};

struct srstd_bridge_trigger_match_shadow {
    void *channel;   /* 上游 struct sr_channel*;Task 9 编码为 tagged pointer
                      * (void*)(intptr_t)(index+1),fix_channels 解码后替换为真实指针 */
    int   match;
    float value;
};

/* === 类型映射辅助函数 ===
 * 上游与 PXView 的 enum 值不完全相同(见文件头注释),按名称语义映射。 */

static int map_channel_type(int up_type)
{
    switch (up_type) {
    case UP_SR_CHANNEL_LOGIC:  return SR_CHANNEL_LOGIC;   /* 10000 → 10000 */
    case UP_SR_CHANNEL_ANALOG: return SR_CHANNEL_ANALOG;  /* 10001 → 10002 */
    default:                   return SR_CHANNEL_LOGIC;   /* 未知类型默认 LOGIC */
    }
}

static int map_packet_type(int up_type, int *ok)
{
    *ok = 1;
    switch (up_type) {
    case UP_SR_DF_HEADER:      return SR_DF_HEADER;      /* 10000 → 10000 */
    case UP_SR_DF_END:         return SR_DF_END;         /* 10001 → 10001 */
    case UP_SR_DF_META:        return SR_DF_META;        /* 10002 → 10002 */
    case UP_SR_DF_TRIGGER:     return SR_DF_TRIGGER;     /* 10003 → 10003 */
    case UP_SR_DF_LOGIC:       return SR_DF_LOGIC;       /* 10004 → 10004 */
    case UP_SR_DF_FRAME_BEGIN: return SR_DF_FRAME_BEGIN; /* 10005 → 10007 */
    case UP_SR_DF_FRAME_END:   return SR_DF_FRAME_END;   /* 10006 → 10008 */
    case UP_SR_DF_ANALOG:      return SR_DF_ANALOG;      /* 10007 → 10006 */
    default: *ok = 0; return -1;
    }
}

static int map_device_status(int up_status)
{
    /* 上游无 SR_ST_INCOMPATIBLE,PXView 有;ACTIVE/STOPPING 值差 1 */
    switch (up_status) {
    case UP_SR_ST_NOT_FOUND:     return SR_ST_NOT_FOUND;      /* 10000 → 10000 */
    case UP_SR_ST_INITIALIZING:  return SR_ST_INITIALIZING;   /* 10001 → 10001 */
    case UP_SR_ST_INACTIVE:      return SR_ST_INACTIVE;       /* 10002 → 10002 */
    case UP_SR_ST_ACTIVE:        return SR_ST_ACTIVE;         /* 10003 → 10004 */
    case UP_SR_ST_STOPPING:      return SR_ST_STOPPING;       /* 10004 → 10005 */
    default:                     return SR_ST_NOT_FOUND;      /* 未知默认 NOT_FOUND */
    }
}

/* === 转换函数 === */

void srstd_channel_to_pxview(const void *src, struct sr_channel *dst)
{
    const struct srstd_bridge_channel_shadow *s;

    if (!src || !dst) return;

    s = (const struct srstd_bridge_channel_shadow *)src;

    dst->index = (uint16_t)s->index;
    dst->type = map_channel_type(s->type);
    dst->enabled = s->enabled;
    dst->name = s->name ? g_strdup(s->name) : NULL;
    /* PXView 专有字段(trigger/bits/vdiv/vfactor/offset/cali_fgain/vga_ptr 等)
     * 保持 alloc 时的 0/NULL,由调用方按需设置。 */
}

void srstd_sdi_to_pxview(const void *src, struct sr_dev_inst *dst)
{
    const struct srstd_bridge_sdi_shadow *s;
    GSList *l;

    if (!src || !dst) return;

    s = (const struct srstd_bridge_sdi_shadow *)src;

    dst->status = map_device_status(s->status);
    dst->inst_type = s->inst_type; /* SR_INST_* 值两库一致,直接拷贝 */
    dst->vendor = s->vendor ? g_strdup(s->vendor) : NULL;
    dst->model = s->model ? g_strdup(s->model) : NULL;
    dst->version = s->version ? g_strdup(s->version) : NULL;
    dst->serial_num = s->serial_num ? g_strdup(s->serial_num) : NULL;
    dst->connection_id = s->connection_id ? g_strdup(s->connection_id) : NULL;

    /* 递归转换 channels 列表:每个上游 channel → 新建 PXView channel */
    for (l = s->channels; l; l = l->next) {
        struct sr_channel *px_ch = srstd_bridge_pxview_channel_alloc();
        if (!px_ch) continue;
        srstd_channel_to_pxview(l->data, px_ch);
        dst->channels = g_slist_append(dst->channels, px_ch);
    }
}

int srstd_packet_to_pxview(const void *src, struct sr_datafeed_packet *dst)
{
    const struct srstd_bridge_packet_shadow *s;
    int ok;
    int px_type;

    if (!src || !dst) return -1;

    s = (const struct srstd_bridge_packet_shadow *)src;
    px_type = map_packet_type(s->type, &ok);
    if (!ok) return -1;

    dst->type = (uint16_t)px_type;
    dst->status = 0;
    dst->bExportOriginalData = 0;
    dst->payload = NULL;

    if (px_type == SR_DF_LOGIC && s->payload) {
        const struct srstd_bridge_logic_shadow *sl;
        struct sr_datafeed_logic *px_logic;

        sl = (const struct srstd_bridge_logic_shadow *)s->payload;
        px_logic = g_new0(struct sr_datafeed_logic, 1);
        px_logic->length = sl->length;
        px_logic->unitsize = sl->unitsize;
        px_logic->data = sl->data; /* 共享指针,不拷贝(_free 不释放) */
        px_logic->format = LA_CROSS_DATA; /* 默认交叉数据格式 */
        dst->payload = px_logic;
    } else if (px_type == SR_DF_ANALOG && s->payload) {
        const struct srstd_bridge_analog_shadow *sa;
        struct sr_datafeed_analog *px_analog;

        sa = (const struct srstd_bridge_analog_shadow *)s->payload;
        px_analog = g_new0(struct sr_datafeed_analog, 1);
        px_analog->data = sa->data; /* 共享指针 */
        px_analog->num_samples = (int)sa->num_samples;
        /* encoding/meaning/spec 布局不兼容,不转换;mq/unit 等保持 0 */
        dst->payload = px_analog;
    }
    /* 其他包类型(HEADER/META/TRIGGER/FRAME_BEGIN/FRAME_END/END):
     * payload 置 NULL。PXView 从自身 session 重建这些元数据,不需要上游 payload。 */

    return 0;
}

/* === PXView 侧结构分配/释放 === */

struct sr_channel *srstd_bridge_pxview_channel_alloc(void)
{
    return g_new0(struct sr_channel, 1);
}

struct sr_dev_inst *srstd_bridge_pxview_sdi_alloc(void)
{
    return g_new0(struct sr_dev_inst, 1);
}

struct sr_datafeed_packet *srstd_bridge_pxview_packet_alloc(void)
{
    return g_new0(struct sr_datafeed_packet, 1);
}

void srstd_bridge_pxview_channel_free(struct sr_channel *dst)
{
    if (!dst) return;
    g_free(dst->name);
    g_free(dst->trigger);
    /* vga_ptr 由 PXView 驱动拥有,bridge alloc 时为 NULL,不释放 */
    g_free(dst);
}

void srstd_bridge_pxview_sdi_free(struct sr_dev_inst *dst)
{
    if (!dst) return;
    g_free(dst->name);
    g_free(dst->path);
    g_free(dst->vendor);
    g_free(dst->version);
    g_free(dst->model);
    g_free(dst->serial_num);
    g_free(dst->connection_id);
    /* channels 列表中的 sr_channel 由 sdi 拥有,递归释放 */
    if (dst->channels) {
        g_slist_free_full(dst->channels,
                          (GDestroyNotify)srstd_bridge_pxview_channel_free);
        dst->channels = NULL;
    }
    /* channel_groups 元素不由 bridge 拥有(alloc 时为 NULL),只释放链表 */
    g_slist_free(dst->channel_groups);
    g_free(dst);
}

void srstd_bridge_pxview_packet_free(struct sr_datafeed_packet *dst)
{
    if (!dst) return;
    if (dst->payload) {
        /* LOGIC/ANALOG:释放 payload 结构体本身。
         * data 指针共享自上游,不释放(所有权归上游)。
         * payload 字段是 const void *,g_free 需要 gpointer,强转去除 const。 */
        g_free((gpointer)dst->payload);
    }
    g_free(dst);
}

size_t srstd_bridge_pxview_channel_size(void)
{
    return sizeof(struct sr_channel);
}

size_t srstd_bridge_pxview_sdi_size(void)
{
    return sizeof(struct sr_dev_inst);
}

size_t srstd_bridge_pxview_packet_size(void)
{
    return sizeof(struct sr_datafeed_packet);
}

/* === PXView 侧字段访问器 === */

int srstd_bridge_pxview_channel_get_index(const struct sr_channel *dst)
{
    return dst ? (int)dst->index : -1;
}

int srstd_bridge_pxview_channel_get_type(const struct sr_channel *dst)
{
    return dst ? dst->type : -1;
}

gboolean srstd_bridge_pxview_channel_get_enabled(const struct sr_channel *dst)
{
    return dst ? dst->enabled : FALSE;
}

const char *srstd_bridge_pxview_channel_get_name(const struct sr_channel *dst)
{
    return dst ? dst->name : NULL;
}

uint64_t srstd_bridge_pxview_channel_get_vdiv(const struct sr_channel *dst)
{
    return dst ? dst->vdiv : 0;
}

const void *srstd_bridge_pxview_channel_get_vga_ptr(const struct sr_channel *dst)
{
    return dst ? (const void *)dst->vga_ptr : NULL;
}

const char *srstd_bridge_pxview_sdi_get_vendor(const struct sr_dev_inst *dst)
{
    return dst ? dst->vendor : NULL;
}

const char *srstd_bridge_pxview_sdi_get_model(const struct sr_dev_inst *dst)
{
    return dst ? dst->model : NULL;
}

guint srstd_bridge_pxview_sdi_get_channel_count(const struct sr_dev_inst *dst)
{
    return dst ? g_slist_length(dst->channels) : 0;
}

GSList *srstd_bridge_pxview_sdi_get_channels(const struct sr_dev_inst *dst)
{
    return dst ? dst->channels : NULL;
}

int srstd_bridge_pxview_sdi_get_status(const struct sr_dev_inst *dst)
{
    return dst ? dst->status : 0;
}

int srstd_bridge_pxview_packet_get_type(const struct sr_datafeed_packet *dst)
{
    return dst ? (int)dst->type : -1;
}

uint64_t srstd_bridge_pxview_packet_logic_get_length(
    const struct sr_datafeed_packet *dst)
{
    if (!dst || dst->type != SR_DF_LOGIC || !dst->payload) return 0;
    return ((const struct sr_datafeed_logic *)dst->payload)->length;
}

uint16_t srstd_bridge_pxview_packet_logic_get_unitsize(
    const struct sr_datafeed_packet *dst)
{
    if (!dst || dst->type != SR_DF_LOGIC || !dst->payload) return 0;
    return ((const struct sr_datafeed_logic *)dst->payload)->unitsize;
}

const void *srstd_bridge_pxview_packet_logic_get_data(
    const struct sr_datafeed_packet *dst)
{
    if (!dst || dst->type != SR_DF_LOGIC || !dst->payload) return NULL;
    return ((const struct sr_datafeed_logic *)dst->payload)->data;
}

int srstd_bridge_pxview_packet_logic_get_format(
    const struct sr_datafeed_packet *dst)
{
    if (!dst || dst->type != SR_DF_LOGIC || !dst->payload) return 0;
    return ((const struct sr_datafeed_logic *)dst->payload)->format;
}

int srstd_bridge_pxview_packet_logic_get_index(
    const struct sr_datafeed_packet *dst)
{
    if (!dst || dst->type != SR_DF_LOGIC || !dst->payload) return 0;
    return (int)((const struct sr_datafeed_logic *)dst->payload)->index;
}

int srstd_bridge_pxview_packet_logic_get_data_error(
    const struct sr_datafeed_packet *dst)
{
    if (!dst || dst->type != SR_DF_LOGIC || !dst->payload) return 0;
    return (int)((const struct sr_datafeed_logic *)dst->payload)->data_error;
}

/* === 触发配置转换(PXView TriggerConfig → 上游 sr_trigger)=== */

/* 触发值字符到 SR_TRIGGER_* 映射。
 * PXView 触发值字符串(per-channel,空格分隔)中的字符:
 *   '0' = 低电平, '1' = 高电平, 'R' = 上升沿, 'F' = 下降沿,
 *   'E' / 'X' / 'x' = 任意(忽略,'X' 跳过不创建 match),
 *   'C' = 时钟边沿(无对应,用 EDGE)。
 * 返回 0 表示"跳过该 channel"(对应 'X'/'x'/未知字符)。 */
static int trigger_char_to_srstd_match(char c)
{
    switch (c) {
    case '0': return UP_SR_TRIGGER_ZERO;     /* 1 */
    case '1': return UP_SR_TRIGGER_ONE;      /* 2 */
    case 'R': return UP_SR_TRIGGER_RISING;   /* 3 */
    case 'F': return UP_SR_TRIGGER_FALLING;  /* 4 */
    case 'E': return UP_SR_TRIGGER_EDGE;     /* 5 */
    case 'C': return UP_SR_TRIGGER_EDGE;     /* clock,用 EDGE */
    default:  return 0;                      /* 'X'/'x'/其他 → 跳过 */
    }
}

/* 解析 per-channel 触发值字符串(形如 "1 0 X R F"),为每个非 'X' 的 channel
 * 创建一个 sr_trigger_match,加入 matches 链表并返回。
 *
 * channel 指针编码:由于 'X' token 被跳过(不创建 match),match 在链表中的
 * 位置不等于 channel index。为让 Task 9 的 srstd_glue_trigger_fix_channels
 * 能恢复正确 channel index,这里把 channel index 编码到 match->channel 字段
 * 作为 tagged pointer:(void*)(intptr_t)(index + 1)。index+1 是为了区分
 * channel 0 与 NULL(0 表示未设置)。fix_channels 解码后替换为真实上游
 * sr_channel 指针。
 *
 * 字符串生命周期由调用方所有;本函数只读不持有。 */
static GSList *parse_trigger_value_string(const char *value_str)
{
    GSList *matches = NULL;
    char *copy;
    char *saveptr = NULL;
    char *tok;
    int channel_index = 0;  /* 每个 token 对应一个 channel,包括 'X' */

    if (!value_str || !*value_str) return NULL;

    /* 拷贝字符串以便 strtok_r 切分(原串是 const,不能就地修改) */
    copy = g_strdup(value_str);
    if (!copy) return NULL;

    tok = strtok_r(copy, " \t", &saveptr);
    while (tok) {
        char c = tok[0];
        int m = trigger_char_to_srstd_match(c);
        if (m != 0) {
            struct srstd_bridge_trigger_match_shadow *match;
            match = g_new0(struct srstd_bridge_trigger_match_shadow, 1);
            /* 编码 channel index:fix_channels 解码 (intptr_t)channel - 1 */
            match->channel = (void *)(intptr_t)(channel_index + 1);
            match->match = m;
            match->value = 0.0f;
            matches = g_slist_append(matches, match);
        }
        channel_index++;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    g_free(copy);
    return matches;
}

/* 构造一个 trigger stage,从 stage_src->value0 / value1 解析 matches。
 * value0 是主触发值,value1 是辅助(通常全 'X',产生 0 个 match)。
 * 两个字符串解析出的 matches 顺序拼接(value0 在前,value1 在后)。 */
static struct srstd_bridge_trigger_stage_shadow *make_trigger_stage(
        int stage_idx,
        const struct pxview_trigger_stage_src *stage_src)
{
    struct srstd_bridge_trigger_stage_shadow *stage;
    GSList *m;

    stage = g_new0(struct srstd_bridge_trigger_stage_shadow, 1);
    stage->stage = stage_idx;
    stage->matches = NULL;

    if (stage_src->value0) {
        m = parse_trigger_value_string(stage_src->value0);
        if (m) stage->matches = g_slist_concat(stage->matches, m);
    }
    if (stage_src->value1) {
        m = parse_trigger_value_string(stage_src->value1);
        if (m) stage->matches = g_slist_concat(stage->matches, m);
    }

    return stage;
}

int pxview_trigger_to_srstd(const struct pxview_trigger_src *src,
                            void *dst_trigger)
{
    struct srstd_bridge_trigger_shadow *trig;
    int i;

    if (!src || !dst_trigger) return -1;
    if (src->stage_count < 1 || !src->stages) return -1;

    trig = (struct srstd_bridge_trigger_shadow *)dst_trigger;

    /* 假设 dst 是 sr_trigger_new/g_new0 新建的,stages 为 NULL。
     * 不清理已有 stages(避免误释放调用方管理的链表)。 */
    trig->stages = NULL;

    switch (src->mode) {
    case 0: /* Simple: 单 stage,从 stages[0].value0 解析 per-channel matches */
    {
        struct srstd_bridge_trigger_stage_shadow *stage;
        stage = make_trigger_stage(0, &src->stages[0]);
        trig->stages = g_slist_append(trig->stages, stage);
        return 0;
    }

    case 1: /* Adv: 多 stage,每 stage 从 value0/value1 解析 */
        for (i = 0; i < src->stage_count; i++) {
            struct srstd_bridge_trigger_stage_shadow *stage;
            stage = make_trigger_stage(i, &src->stages[i]);
            trig->stages = g_slist_append(trig->stages, stage);
        }
        return 0;

    case 2: /* Serial: 4 stage 固定结构,暂未实现 */
        return -2;

    default:
        return -1;
    }
}
