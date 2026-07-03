/*
 * test_srstd_init_shared.c - 验证 libusb_context 共享
 *
 * 测试流程:
 *   1. libusb_init(&local_usb_ctx) 创建本地 libusb_context(模拟 PXView 创建的)
 *   2. srstd_init_shared(&srstd_ctx, local_usb_ctx) 创建共享 sr_context
 *   3. 断言 srstd_ctx->libusb_ctx == local_usb_ctx
 *   4. 先把 libusb_ctx 置 NULL,再 srstd_exit(避免释放共享的 PXView context)
 *   5. libusb_exit(local_usb_ctx) 释放本地 context
 *
 * 编译说明:
 *   本测试通过 -include srstd_rename.h 编译,所以源码中 sr_init/sr_exit/
 *   struct sr_context 会被预处理为 srstd_init/srstd_exit/struct srstd_context。
 *   srstd_init_shared 是我们新增的函数,不匹配 sr_* 模式,不会被重命名。
 *   SR_OK 是 enum 值(非 enum 标签),不会被重命名。
 *   需要包含 libsigrok-internal.h 以访问 sr_context 的 libusb_ctx 字段
 *   (libsigrok.h 中只有前向声明 struct sr_context;)。
 *
 * Include path 注意:
 *   CMakeLists.txt 中 test target 使用 BEFORE PRIVATE 强制 libsigrokstd/include
 *   和 libsigrokstd/src 排在全局 ./libsigrok (PXView fork) 之前,否则会引用
 *   PXView 的 struct sr_context 布局(libusb_ctx 在偏移 0),与上游布局
 *   (libusb_ctx 在偏移 8,driver_list 之后)不匹配,导致指针读取错误。
 */

#include "srstd.h"              /* 包含 libsigrok.h + srstd_rename.h */
#include "libsigrok-internal.h" /* struct sr_context 完整定义(含 libusb_ctx 字段) */
#include <libusb.h>
#include <stdio.h>

int main(void)
{
    struct sr_context *srstd_ctx = NULL;
    libusb_context *local_usb_ctx = NULL;
    int ret;

    /* 无缓冲输出,崩溃时也能看到已打印的信息 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* 步骤 1: 创建本地 libusb_context(模拟 PXView 创建的) */
    ret = libusb_init(&local_usb_ctx);
    if (ret != 0) {
        printf("FAIL: libusb_init failed: %d\n", ret);
        return 1;
    }
    printf("OK: local libusb_context created at %p\n", (void*)local_usb_ctx);

    /* 步骤 2: srstd_init_shared 共享 libusb_context */
    ret = srstd_init_shared(&srstd_ctx, local_usb_ctx);
    if (ret != SR_OK) {
        printf("FAIL: srstd_init_shared failed: %d\n", ret);
        libusb_exit(local_usb_ctx);
        return 1;
    }
    printf("OK: srstd_init_shared returned SR_OK\n");

    /* 步骤 3: 验证 libusb_ctx 指针相等 */
    if (srstd_ctx->libusb_ctx != local_usb_ctx) {
        printf("FAIL: libusb_ctx mismatch: srstd_ctx=%p vs local=%p\n",
               (void*)srstd_ctx->libusb_ctx, (void*)local_usb_ctx);
        /* 失败路径也要安全清理:置 NULL 避免 srstd_exit 释放 local_usb_ctx */
        srstd_ctx->libusb_ctx = NULL;
        srstd_exit(srstd_ctx);
        libusb_exit(local_usb_ctx);
        return 1;
    }
    printf("OK: libusb_ctx shared correctly: %p == %p\n",
           (void*)srstd_ctx->libusb_ctx, (void*)local_usb_ctx);

    /*
     * 步骤 4: 先把 libusb_ctx 置 NULL,再 srstd_exit
     * 这模拟 PXView 实际使用场景:共享的 libusb_context 由 PXView 自己管理生命周期,
     * srstd_exit 不应该释放它。libusb_exit(NULL) 是安全的(libusb 文档明确允许)。
     */
    srstd_ctx->libusb_ctx = NULL;
    srstd_exit(srstd_ctx);
    printf("OK: srstd_exit completed without crash (libusb_exit(NULL) safe)\n");

    /* 步骤 5: 释放本地 context */
    libusb_exit(local_usb_ctx);
    printf("OK: local libusb_context released\n");

    printf("\nALL TESTS PASSED\n");
    return 0;
}
