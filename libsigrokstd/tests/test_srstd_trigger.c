/*
 * test_srstd_trigger.c - 验证 pxview_trigger_to_srstd() 触发配置转换
 *
 * 测试 PXView TriggerConfig 的 C 中间表示(pxview_trigger_src)→
 * 上游 sr_trigger 结构的转换,覆盖 Simple/Adv/Serial 三种模式。
 *
 * === 编译说明 ===
 *
 * 本测试通过 -include srstd_rename.h 编译,所以源码中的:
 *   struct sr_trigger       → struct srstd_trigger       (上游 2 字段)
 *   struct sr_trigger_stage → struct srstd_trigger_stage (上游 2 字段)
 *   struct sr_trigger_match → struct srstd_trigger_match (上游 3 字段,含 float value)
 *   sr_trigger_new          → srstd_trigger_new
 *   sr_trigger_free         → srstd_trigger_free
 *
 * SR_TRIGGER_ZERO / SR_TRIGGER_ONE / SR_TRIGGER_RISING / SR_TRIGGER_FALLING
 * 等大写 enum 常量 **不** 被重命名(srstd_rename.h 只重命名 sr_* 小写前缀的符号),
 * 所以它们保持上游值:ZERO=1, ONE=2, RISING=3, FALLING=4, EDGE=5, OVER=6, UNDER=7。
 *
 * 上游 sr_trigger 布局与本库 srstd_bridge.c 中的 srstd_bridge_trigger_shadow
 * 影子结构一致,所以本测试可以直接解引用 trig->stages 等字段验证。
 *
 * === channel 指针编码(Task 9)===
 *
 * Simple/Adv 模式下,match->channel 不再是 NULL,而是编码了 channel index 的
 * tagged pointer:(void*)(intptr_t)(index + 1)。这是为了让 Task 9 的
 * srstd_glue_trigger_fix_channels 能恢复正确的 channel index(因为 'X' token
 * 被跳过,match 在链表中的位置不等于 channel index)。fix_channels 解码后
 * 替换为真实上游 sr_channel 指针。
 * 本测试只验证结构生成正确(含 tagged pointer 编码),不调用 fix_channels。
 */

#include "srstd.h"              /* 上游 libsigrok.h + srstd_rename.h */
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* srstd_bridge.h 提供 pxview_trigger_src + pxview_trigger_to_srstd()。
 * 注意:srstd_bridge.h 内的 struct sr_channel 等前向声明会被重命名为
 * struct srstd_channel,但 pxview_trigger_src 不含任何 sr_ 前缀类型字段
 * (只有 int 与 const char 指针与数组),所以不受重命名影响。 */
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

/* === 测试 1: Simple 模式 ===
 *
 * 构造 pxview_trigger_src(mode=0, stages[0].value0="1 0 X R F"),
 * 验证生成的 trigger:
 *   - 1 个 stage(stage==0)
 *   - 4 个 match(跳过 'X')
 *   - match 顺序:[ONE=2, ZERO=1, RISING=3, FALLING=4]
 *   - match->channel == tagged pointer (void*)(intptr_t)(index+1)
 *     (ch0=1, ch1=2, ch3=4, ch4=5 — ch2 被 'X' 跳过)
 *   - match->value == 0.0f
 */
static void test_simple_mode(void)
{
    struct pxview_trigger_stage_src stage;
    struct pxview_trigger_src src;
    struct sr_trigger *trig;
    int ret;

    printf("\n=== Test 1: Simple mode ===\n");

    /* 构造 pxview_trigger_src
     * value0="1 0 X R F" 表示 5 个 channel 的触发条件:
     *   ch0='1'(高电平), ch1='0'(低电平), ch2='X'(任意,跳过),
     *   ch3='R'(上升沿), ch4='F'(下降沿) */
    memset(&stage, 0, sizeof(stage));
    stage.value0 = "1 0 X R F";
    stage.value1 = NULL;  /* Simple 模式无 value1 */
    stage.logic = 0;
    stage.inv0 = 0;
    stage.inv1 = 0;
    stage.count0 = 0;
    stage.count1 = 0;

    memset(&src, 0, sizeof(src));
    src.mode = 0;         /* Simple */
    src.stage_count = 1;
    src.stages = &stage;

    /* 上游 sr_trigger_new 分配空 trigger(srstd_rename.h 重命名为 srstd_trigger_new) */
    trig = sr_trigger_new(NULL);
    CHECK(trig != NULL, "sr_trigger_new returned non-NULL");
    CHECK(trig->name == NULL, "trigger name == NULL (passed NULL to sr_trigger_new)");
    CHECK(trig->stages == NULL, "trigger stages == NULL before conversion");

    /* 转换 */
    ret = pxview_trigger_to_srstd(&src, trig);
    CHECK(ret == 0, "Simple mode conversion returned 0 (success)");

    /* 验证 stage 数量 == 1 */
    CHECK(g_slist_length(trig->stages) == 1,
          "Simple: trigger has 1 stage");

    /* 验证 stage 字段 */
    if (trig->stages && trig->stages->data) {
        struct sr_trigger_stage *st = trig->stages->data;
        CHECK(st->stage == 0, "Simple: stage index == 0");
        CHECK(g_slist_length(st->matches) == 4,
              "Simple: stage has 4 matches (skipped 'X')");

        /* 验证 match 顺序:[ONE=2, ZERO=1, RISING=3, FALLING=4]
         * 和 channel tagged pointer 编码(ch0=1,ch1=2,ch3=4,ch4=5) */
        if (g_slist_length(st->matches) == 4) {
            GSList *l;
            struct sr_trigger_match *m;
            int expected_match[4] = {SR_TRIGGER_ONE, SR_TRIGGER_ZERO,
                                     SR_TRIGGER_RISING, SR_TRIGGER_FALLING};
            /* expected channel tagged pointers: (void*)(intptr_t)(index+1)
             * value0="1 0 X R F" → ch0='1', ch1='0', ch2='X'(skip), ch3='R', ch4='F'
             * tagged = index+1 → ch0=1, ch1=2, ch3=4, ch4=5 */
            intptr_t expected_ch[4] = {1, 2, 4, 5};
            int idx = 0;
            int all_match_ok = 1;
            for (l = st->matches; l && idx < 4; l = l->next, idx++) {
                m = l->data;
                if (!m) { all_match_ok = 0; break; }
                if (m->match != expected_match[idx]) {
                    printf("  match[%d]: expected match %d, got %d\n",
                           idx, expected_match[idx], m->match);
                    all_match_ok = 0;
                }
                /* channel 应为 tagged pointer (void*)(intptr_t)(index+1) */
                if ((intptr_t)m->channel != expected_ch[idx]) {
                    printf("  match[%d]: expected channel tag %ld, got %ld\n",
                           idx, (long)expected_ch[idx], (long)(intptr_t)m->channel);
                    all_match_ok = 0;
                }
                /* value 应为 0.0f(logic channel 无值) */
                if (m->value != 0.0f) {
                    printf("  match[%d]: value != 0.0f (got %f)\n", idx, m->value);
                    all_match_ok = 0;
                }
            }
            CHECK(all_match_ok,
                  "Simple: match values + channel tags + value==0.0f");
        }
    } else {
        CHECK(0, "Simple: stage is non-NULL (FAIL: got NULL)");
    }

    /* 清理:sr_trigger_free 会递归释放 stages 和 matches
     * (srstd_rename.h 重命名为 srstd_trigger_free)。
     * 注意:match->channel 是 tagged pointer(非真实指针),上游 sr_trigger_free
     * 不释放 channel 字段(只释放 match 结构本身),所以安全。 */
    sr_trigger_free(trig);

    printf("Test 1: Simple mode done\n");
}

/* === 测试 2: Adv 模式(2 stage)===
 *
 * 构造 pxview_trigger_src(mode=1, 2 stages),每 stage 有 value0,
 * 验证生成的 trigger 含 2 个 stage,stage 索引分别为 0 和 1,
 * 每 stage 的 matches 数量正确。
 */
static void test_adv_mode(void)
{
    struct pxview_trigger_stage_src stages[2];
    struct pxview_trigger_src src;
    struct sr_trigger *trig;
    int ret;

    printf("\n=== Test 2: Adv mode (2 stages) ===\n");

    /* stage 0:value0="1 0 R"(3 个非 X channel) */
    memset(&stages[0], 0, sizeof(stages[0]));
    stages[0].value0 = "1 0 R";
    stages[0].value1 = NULL;
    stages[0].logic = 0;
    stages[0].inv0 = 0;
    stages[0].inv1 = 0;
    stages[0].count0 = 0;
    stages[0].count1 = 0;

    /* stage 1:value0="0 1 F"(3 个非 X channel) */
    memset(&stages[1], 0, sizeof(stages[1]));
    stages[1].value0 = "0 1 F";
    stages[1].value1 = NULL;
    stages[1].logic = 0;
    stages[1].inv0 = 0;
    stages[1].inv1 = 0;
    stages[1].count0 = 0;
    stages[1].count1 = 0;

    memset(&src, 0, sizeof(src));
    src.mode = 1;         /* Adv */
    src.stage_count = 2;
    src.stages = stages;

    trig = sr_trigger_new(NULL);
    CHECK(trig != NULL, "sr_trigger_new returned non-NULL");

    ret = pxview_trigger_to_srstd(&src, trig);
    CHECK(ret == 0, "Adv mode conversion returned 0 (success)");

    /* 验证 stage 数量 == 2 */
    CHECK(g_slist_length(trig->stages) == 2,
          "Adv: trigger has 2 stages");

    /* 验证每 stage 的索引和 matches */
    if (g_slist_length(trig->stages) == 2) {
        GSList *l;
        int idx = 0;
        int all_ok = 1;
        for (l = trig->stages; l; l = l->next, idx++) {
            struct sr_trigger_stage *st = l->data;
            if (!st) { all_ok = 0; break; }
            if (st->stage != idx) {
                printf("  stage[%d]: index == %d (expected %d)\n",
                       idx, st->stage, idx);
                all_ok = 0;
            }
            if (g_slist_length(st->matches) != 3) {
                printf("  stage[%d]: matches == %u (expected 3)\n",
                       idx, g_slist_length(st->matches));
                all_ok = 0;
            }
        }
        CHECK(all_ok, "Adv: stage indices [0,1], each has 3 matches");
    }

    sr_trigger_free(trig);

    printf("Test 2: Adv mode done\n");
}

/* === 测试 3: Adv 模式 value0+value1 拼接 ===
 *
 * stage 同时有 value0="1 0" 和 value1="R F",验证 matches 顺序拼接:
 *   [ONE, ZERO, RISING, FALLING](value0 在前,value1 在后)
 */
static void test_adv_value1_concat(void)
{
    struct pxview_trigger_stage_src stage;
    struct pxview_trigger_src src;
    struct sr_trigger *trig;

    printf("\n=== Test 3: Adv mode value0+value1 concat ===\n");

    memset(&stage, 0, sizeof(stage));
    stage.value0 = "1 0";
    stage.value1 = "R F";

    memset(&src, 0, sizeof(src));
    src.mode = 1;
    src.stage_count = 1;
    src.stages = &stage;

    trig = sr_trigger_new(NULL);
    pxview_trigger_to_srstd(&src, trig);

    CHECK(g_slist_length(trig->stages) == 1,
          "Adv value1: trigger has 1 stage");

    if (g_slist_length(trig->stages) == 1) {
        struct sr_trigger_stage *st = trig->stages->data;
        CHECK(g_slist_length(st->matches) == 4,
              "Adv value1: stage has 4 matches (value0=2 + value1=2)");

        /* 验证顺序:value0 的 [ONE, ZERO] 在前,value1 的 [RISING, FALLING] 在后 */
        if (g_slist_length(st->matches) == 4) {
            GSList *l;
            int expected[4] = {SR_TRIGGER_ONE, SR_TRIGGER_ZERO,
                               SR_TRIGGER_RISING, SR_TRIGGER_FALLING};
            int idx = 0;
            int all_ok = 1;
            for (l = st->matches; l && idx < 4; l = l->next, idx++) {
                struct sr_trigger_match *m = l->data;
                if (!m || m->match != expected[idx]) {
                    all_ok = 0;
                }
            }
            CHECK(all_ok, "Adv value1: match order [ONE,ZERO,RISING,FALLING] (value0 then value1)");
        }
    }

    sr_trigger_free(trig);
    printf("Test 3: Adv value1 concat done\n");
}

/* === 测试 4: Serial 模式(未实现,返回 -2)===
 *
 * 构造 mode=2 的 pxview_trigger_src,验证 pxview_trigger_to_srstd 返回 -2。
 * trigger 状态不应被修改(stages 保持 NULL)。
 */
static void test_serial_mode_unimplemented(void)
{
    struct pxview_trigger_stage_src dummy_stages[4];
    struct pxview_trigger_src src;
    struct sr_trigger *trig;
    int ret;

    printf("\n=== Test 4: Serial mode (unimplemented, expect -2) ===\n");

    /* Serial 模式 4 stage,字段大部分不填(本测试只验证返回值) */
    memset(dummy_stages, 0, sizeof(dummy_stages));
    memset(&src, 0, sizeof(src));
    src.mode = 2;         /* Serial */
    src.stage_count = 4;
    src.stages = dummy_stages;
    src.serial_data_channel = 0;
    src.serial_bits = 8;
    src.serial_value = "FF";

    trig = sr_trigger_new(NULL);
    CHECK(trig != NULL, "sr_trigger_new returned non-NULL");

    ret = pxview_trigger_to_srstd(&src, trig);
    CHECK(ret == -2, "Serial mode returns -2 (unimplemented)");

    /* trigger 状态不应被修改 */
    CHECK(trig->stages == NULL,
          "Serial: trigger stages remains NULL (no partial fill)");

    sr_trigger_free(trig);
    printf("Test 4: Serial mode done\n");
}

/* === 测试 5: NULL 参数与边界检查 ===
 *
 * 验证 NULL src / NULL dst / stage_count<1 都返回 -1。
 */
static void test_null_and_boundary(void)
{
    struct pxview_trigger_stage_src stage;
    struct pxview_trigger_src src;
    struct sr_trigger *trig;
    int ret;

    printf("\n=== Test 5: NULL and boundary checks ===\n");

    /* NULL src */
    trig = sr_trigger_new(NULL);
    ret = pxview_trigger_to_srstd(NULL, trig);
    CHECK(ret == -1, "NULL src returns -1");
    sr_trigger_free(trig);

    /* NULL dst */
    memset(&stage, 0, sizeof(stage));
    stage.value0 = "1 0";
    memset(&src, 0, sizeof(src));
    src.mode = 0;
    src.stage_count = 1;
    src.stages = &stage;
    ret = pxview_trigger_to_srstd(&src, NULL);
    CHECK(ret == -1, "NULL dst returns -1");

    /* stage_count < 1 */
    memset(&src, 0, sizeof(src));
    src.mode = 0;
    src.stage_count = 0;
    src.stages = &stage;
    trig = sr_trigger_new(NULL);
    ret = pxview_trigger_to_srstd(&src, trig);
    CHECK(ret == -1, "stage_count==0 returns -1");
    sr_trigger_free(trig);

    /* NULL stages with stage_count > 0 */
    memset(&src, 0, sizeof(src));
    src.mode = 0;
    src.stage_count = 1;
    src.stages = NULL;
    trig = sr_trigger_new(NULL);
    ret = pxview_trigger_to_srstd(&src, trig);
    CHECK(ret == -1, "NULL stages with stage_count>0 returns -1");
    sr_trigger_free(trig);

    /* 未知 mode */
    memset(&stage, 0, sizeof(stage));
    stage.value0 = "1";
    memset(&src, 0, sizeof(src));
    src.mode = 99;
    src.stage_count = 1;
    src.stages = &stage;
    trig = sr_trigger_new(NULL);
    ret = pxview_trigger_to_srstd(&src, trig);
    CHECK(ret == -1, "unknown mode (99) returns -1");
    sr_trigger_free(trig);

    printf("Test 5: NULL and boundary done\n");
}

/* === 测试 6: 'X' 大小写与空字符串处理 ===
 *
 * 验证 'X'/'x' 都被跳过,空字符串/NULL 字符串产生 0 个 match。
 */
static void test_x_skip_and_empty(void)
{
    struct pxview_trigger_stage_src stage;
    struct pxview_trigger_src src;
    struct sr_trigger *trig;

    printf("\n=== Test 6: 'X'/'x' skip and empty string ===\n");

    /* 全 'X'/'x' → 0 个 match */
    memset(&stage, 0, sizeof(stage));
    stage.value0 = "X x X x";
    memset(&src, 0, sizeof(src));
    src.mode = 0;
    src.stage_count = 1;
    src.stages = &stage;
    trig = sr_trigger_new(NULL);
    pxview_trigger_to_srstd(&src, trig);
    if (g_slist_length(trig->stages) == 1) {
        struct sr_trigger_stage *st = trig->stages->data;
        CHECK(g_slist_length(st->matches) == 0,
              "All 'X'/'x': 0 matches created");
    }
    sr_trigger_free(trig);

    /* 空字符串 → 0 个 match */
    memset(&stage, 0, sizeof(stage));
    stage.value0 = "";
    memset(&src, 0, sizeof(src));
    src.mode = 0;
    src.stage_count = 1;
    src.stages = &stage;
    trig = sr_trigger_new(NULL);
    pxview_trigger_to_srstd(&src, trig);
    if (g_slist_length(trig->stages) == 1) {
        struct sr_trigger_stage *st = trig->stages->data;
        CHECK(g_slist_length(st->matches) == 0,
              "Empty value0: 0 matches created");
    }
    sr_trigger_free(trig);

    /* NULL value0 → 0 个 match */
    memset(&stage, 0, sizeof(stage));
    stage.value0 = NULL;
    memset(&src, 0, sizeof(src));
    src.mode = 0;
    src.stage_count = 1;
    src.stages = &stage;
    trig = sr_trigger_new(NULL);
    pxview_trigger_to_srstd(&src, trig);
    if (g_slist_length(trig->stages) == 1) {
        struct sr_trigger_stage *st = trig->stages->data;
        CHECK(g_slist_length(st->matches) == 0,
              "NULL value0: 0 matches created");
    }
    sr_trigger_free(trig);

    printf("Test 6: X skip and empty done\n");
}

int main(void)
{
    /* 无缓冲输出,崩溃时也能看到已打印的信息 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("=== pxview_trigger_to_srstd conversion tests ===\n");
    printf("Upstream SR_TRIGGER_* values: ZERO=%d ONE=%d RISING=%d FALLING=%d EDGE=%d\n",
           SR_TRIGGER_ZERO, SR_TRIGGER_ONE, SR_TRIGGER_RISING,
           SR_TRIGGER_FALLING, SR_TRIGGER_EDGE);

    test_simple_mode();
    test_adv_mode();
    test_adv_value1_concat();
    test_serial_mode_unimplemented();
    test_null_and_boundary();
    test_x_skip_and_empty();

    printf("\n=== Summary: %d checks, %d failures ===\n", checks, failures);
    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\nTESTS FAILED\n");
    return 1;
}
