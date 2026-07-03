/*
 * srstd_bridge.h - 双库数据结构转换层
 *
 * 把上游 libsigrokstd 的 srstd_dev_inst/srstd_channel/srstd_datafeed_packet
 * 转换为 PXView libsigrok 的对应结构,让 PXView Core 能消费上游驱动数据。
 *
 * === 设计说明(为什么用 void* 接口)===
 *
 * 上游 libsigrokstd 和 PXView libsigrok 都定义了 SR_CHANNEL_LOGIC、SR_DF_LOGIC
 * 等 enum 常量,但值不完全相同(例如 SR_DF_ANALOG 上游=10007,PXView=10006)。
 * C 语言中同一作用域内 enum 常量重定义是编译错误,因此无法在同一 TU 内同时
 * include 两个 libsigrok.h。
 *
 * 解决方案:
 *   - 本头文件不 include 任何 libsigrok.h,只暴露 void* 接口。
 *   - srstd_bridge.c(本库内)include PXView libsigrok.h,并用影子结构
 *     (struct srstd_bridge_*_shadow)匹配上游布局,通过 void* 强转访问。
 *   - 调用方(测试)用 -include srstd_rename.h 编译,直接构造上游 srstd_*
 *     实例,然后强转为 void* 传入。
 *   - PXView 侧结构由本库提供的 alloc/free 管理生命周期,字段读取通过
 *     accessor 函数,避免调用方 include PXView libsigrok.h(否则会与上游
 *     enum 冲突)。
 *
 * 上游结构布局(经 srstd_rename.h 重命名后的字段):
 *   struct srstd_channel       { void *sdi; int index; int type;
 *                                gboolean enabled; char *name; void *priv; }
 *   struct srstd_dev_inst      { void *driver; int status; int inst_type;
 *                                char *vendor/model/version/serial_num/connection_id;
 *                                GSList *channels/channel_groups;
 *                                void *conn/priv; void *session; }
 *   struct srstd_datafeed_packet { uint16_t type; const void *payload; }
 *   struct srstd_datafeed_logic  { uint64_t length; uint16_t unitsize; void *data; }
 *   struct srstd_datafeed_analog { void *data; uint32_t num_samples;
 *                                   void *encoding/meaning/spec; }
 */

#ifndef SRSTD_BRIDGE_H_
#define SRSTD_BRIDGE_H_

#include <glib.h>      /* GSList, gboolean */
#include <stdint.h>    /* uint16_t, uint64_t, int8_t */
#include <stdlib.h>    /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PXView 侧结构前向声明。
 * 注意:不能在这里 include PXView libsigrok.h,否则与上游 enum 常量冲突。
 * 调用方若需要完整定义,需自行 include(但通常不需要,用下面的 alloc/free/accessor)。
 */
struct sr_dev_inst;
struct sr_channel;
struct sr_datafeed_packet;

/* === 转换函数 ===
 *
 * src 指向上游 libsigrokstd 结构(srstd_channel/srstd_dev_inst/srstd_datafeed_packet)。
 *   调用方负责构造上游实例(通常在 -include srstd_rename.h 编译的 TU 中),
 *   然后强转为 const void* 传入。
 * dst 指向 PXView libsigrok 结构(由下面的 _alloc 函数分配)。
 *   函数填充 dst 的字段;PXView 专有字段填默认值(0/NULL)。
 *
 * 返回值:channel/sdi 无返回值;packet 返回 0 成功,负值失败。
 *
 * 内存所有权:
 *   - src:调用方所有,函数只读
 *   - dst:调用方所有(通过 _alloc 分配),调用方负责 _free
 *   - dst 内部新建的 payload(如 logic/analog)由 dst 持有,_free 时释放
 *   - dst->channels 列表中的 sr_channel 节点由 dst 持有,_free 时递归释放
 *   - 字符串字段(vendor/model 等)由 g_strdup 拷贝,_free 时释放
 *   - data 指针(logic/analog)直接共享 src 的指针,不拷贝,_free 不释放
 */
void srstd_channel_to_pxview(const void *src, struct sr_channel *dst);
void srstd_sdi_to_pxview(const void *src, struct sr_dev_inst *dst);
int  srstd_packet_to_pxview(const void *src, struct sr_datafeed_packet *dst);

/* === PXView 侧结构分配/释放 ===
 *
 * 分配零初始化的 PXView 结构。调用方负责用 _free 释放。
 * 提供 size 函数以便调用方在需要时自行分配(如栈上或自定义分配器)。
 */
struct sr_channel        *srstd_bridge_pxview_channel_alloc(void);
struct sr_dev_inst       *srstd_bridge_pxview_sdi_alloc(void);
struct sr_datafeed_packet *srstd_bridge_pxview_packet_alloc(void);

void srstd_bridge_pxview_channel_free(struct sr_channel *dst);
void srstd_bridge_pxview_sdi_free(struct sr_dev_inst *dst);
void srstd_bridge_pxview_packet_free(struct sr_datafeed_packet *dst);

size_t srstd_bridge_pxview_channel_size(void);
size_t srstd_bridge_pxview_sdi_size(void);
size_t srstd_bridge_pxview_packet_size(void);

/* === PXView 侧字段访问器 ===
 *
 * 让调用方(通常是测试)能在不 include PXView libsigrok.h 的情况下读取
 * PXView 结构字段(避免与上游 enum 常量冲突)。
 *
 * channel 字段:
 *   index     - 通道索引(uint16_t,但返回 int 便于断言)
 *   type      - 通道类型(PXView 值:SR_CHANNEL_LOGIC=10000/ANALOG=10002/DSO=10001)
 *   enabled   - 是否启用
 *   name      - 通道名(只读,不转移所有权)
 *   vdiv      - 电压分度(PXView 专有,转换后应为 0)
 *   vga_ptr   - VGA 指针(PXView 专有,转换后应为 NULL)
 */
int          srstd_bridge_pxview_channel_get_index(const struct sr_channel *dst);
int          srstd_bridge_pxview_channel_get_type(const struct sr_channel *dst);
gboolean     srstd_bridge_pxview_channel_get_enabled(const struct sr_channel *dst);
const char  *srstd_bridge_pxview_channel_get_name(const struct sr_channel *dst);
uint64_t     srstd_bridge_pxview_channel_get_vdiv(const struct sr_channel *dst);
const void  *srstd_bridge_pxview_channel_get_vga_ptr(const struct sr_channel *dst);

/* sdi 字段:
 *   vendor/model      - 厂商/型号(只读)
 *   channel_count     - 通道列表长度
 *   status            - 设备状态(PXView SR_ST_NOT_FOUND=10000 等)
 *   channels          - 通道列表 GSList*(PXView sr_channel structs)
 *                       Task 8 Part 3: DeviceAgent::get_channels() reads this
 *                       for LIB_SRSTD devices (shadow sdi's converted channels)
 */
const char  *srstd_bridge_pxview_sdi_get_vendor(const struct sr_dev_inst *dst);
const char  *srstd_bridge_pxview_sdi_get_model(const struct sr_dev_inst *dst);
guint        srstd_bridge_pxview_sdi_get_channel_count(const struct sr_dev_inst *dst);
int          srstd_bridge_pxview_sdi_get_status(const struct sr_dev_inst *dst);
GSList      *srstd_bridge_pxview_sdi_get_channels(const struct sr_dev_inst *dst);

/* packet 字段:
 *   type   - 包类型(PXView 值:SR_DF_LOGIC=10004/ANALOG=10006/FRAME_BEGIN=10007 等)
 *   logic  - 当 type==SR_DF_LOGIC 时,读取 logic payload 的字段
 */
int          srstd_bridge_pxview_packet_get_type(const struct sr_datafeed_packet *dst);
uint64_t     srstd_bridge_pxview_packet_logic_get_length(const struct sr_datafeed_packet *dst);
uint16_t     srstd_bridge_pxview_packet_logic_get_unitsize(const struct sr_datafeed_packet *dst);
const void  *srstd_bridge_pxview_packet_logic_get_data(const struct sr_datafeed_packet *dst);
int          srstd_bridge_pxview_packet_logic_get_format(const struct sr_datafeed_packet *dst);
int          srstd_bridge_pxview_packet_logic_get_index(const struct sr_datafeed_packet *dst);
int          srstd_bridge_pxview_packet_logic_get_data_error(const struct sr_datafeed_packet *dst);

/* === 触发配置转换(PXView TriggerConfig → 上游 sr_trigger)===
 *
 * PXView 的 TriggerConfig 是 C++ 类(QString/std::vector),不能直接传给 C 函数。
 * 调用方(后续 Task 9 在 PXView C++ 层)先把 TriggerConfig 转换为
 * pxview_trigger_src 中间结构(纯 C,字符串指针指向临时缓冲区,生命周期由
 * 调用方管理),再调用 pxview_trigger_to_srstd 把它转换为上游 sr_trigger。
 *
 * dst_trigger 是预分配的上游 sr_trigger(通常由上游 sr_trigger_new 创建,
 * 或 g_new0)。函数填充 stages/matches 链表;调用方负责用上游 sr_trigger_free
 * 释放(srstd_rename.h 编译的 TU 中会被重命名为 srstd_trigger_free)。
 *
 * === channel 指针编码(Task 9 修复)===
 *
 * Simple/Adv 模式下,生成的 match->channel 不是 NULL,而是编码了 channel index
 * 的 tagged pointer:(void*)(intptr_t)(index + 1)。这是因为 'X' token 被跳过
 * (不创建 match),match 在链表中的位置 ≠ channel index,所以需要用 tagged
 * pointer 保存真实 channel index。
 *
 * Task 9 的 srstd_glue_trigger_fix_channels(glue 层)在调用本函数后,遍历
 * trigger->stages[i]->matches[j],解码 tagged pointer 得到 channel index,
 * 从上游 sdi->channels 中按 index 查找真实上游 sr_channel* 填入,替换 tagged
 * pointer。修复后上游 soft_trigger_logic_check 可正常解引用 channel->index /
 * channel->enabled。
 *
 * 返回值:0 成功,负值失败。
 *   -1 = 参数错误(NULL src/dst 或 stage_count<1)
 *   -2 = 模式不支持(目前 Serial 模式未实现)
 */

struct pxview_trigger_stage_src {
    const char *value0;   /* Simple/Adv: per-channel 触发值字符串(如 "1 0 X R F ..."),空格分隔 */
    const char *value1;   /* Simple/Adv: per-channel 触发值字符串(第二套,通常全 'X') */
    int        logic;     /* (contiguous<<1) + logic_index */
    int        inv0;      /* inv0 combobox index */
    int        inv1;      /* inv1 combobox index */
    int        count0;    /* count spinbox value */
    int        count1;    /* 通常 0 */
};

struct pxview_trigger_src {
    int  mode;            /* 0=Simple, 1=Adv, 2=Serial */
    int  stage_count;
    const struct pxview_trigger_stage_src *stages;  /* 数组,长度 stage_count */

    /* Serial 专用(本函数当前未使用,Task 9+ 完善) */
    int  serial_data_channel;
    int  serial_bits;
    const char *serial_value;  /* hex 字符串 */
};

int pxview_trigger_to_srstd(const struct pxview_trigger_src *src,
                            void *dst_trigger);

#ifdef __cplusplus
}
#endif

#endif /* SRSTD_BRIDGE_H_ */
