/*
 * srstd_init_shared.c - libusb_context 共享包装函数
 *
 * 让上游 libsigrokstd 库共享 PXView 已创建的 libusb_context,
 * 避免两库各自 libusb_init() 导致的设备发现/热插拔隔离。
 *
 * Phase 1/2 已删除 srstd_rename.h,源码中所有 sr_* token 直接引用
 * 上游真实符号(sr_init/sr_exit/struct sr_context 等)。
 * srstd_init_shared 函数名本身不匹配 sr_* 模式,保持原名。
 * SR_OK/SR_ERR_ARG 是 enum 值(非 enum 标签),也不受 rename 影响。
 * libusb_* 不在 sr_* 命名范围内,不受影响。
 */

#include <libsigrok/libsigrok.h>  /* 上游 libsigrok.h,通过 libsigrokstd BEFORE include path */
#include "libsigrok-internal.h"   /* struct sr_context 完整定义(含 libusb_ctx 字段) */
#include <libusb.h>

/*
 * Driver registration: libsigrokstd is now built as a SHARED library with
 * the upstream SR_REGISTER_DEV_DRIVER section mechanism. driver_list_start.c
 * and driver_list_stop.c define the section boundaries; each driver's api.c
 * registers itself via SR_REGISTER_DEV_DRIVER(<name>_driver_info), which
 * places a pointer to the driver_info struct in the __sr_driver_list section.
 *
 * sr_drivers_init() (called inside sr_init()) iterates from
 * sr_driver_list__start + 1 to sr_driver_list__stop (exclusive) and builds
 * ctx->driver_list automatically. This requires HAVE_DRIVERS to be defined
 * at compile time (done by CMake target_compile_definitions).
 *
 * The previous driver_registry.c + srstd_get_all_drivers() batch-registration
 * approach has been removed in favor of the unmodified upstream mechanism.
 */

/*
 * srstd_init_shared: 创建上游 sr_context,但共享已有的 libusb_context
 *
 * 参数:
 *   ctx              - 输出参数,返回新建的 srstd_context 指针
 *   shared_usb_ctx   - PXView 已创建的 libusb_context 指针
 *
 * 返回值:
 *   SR_OK(0) 成功,负值失败
 *
 * 流程:
 *   1. 调用 sr_init(ctx) 创建上游 sr_context(内部会 libusb_init 一个新 context)
 *   2. libusb_exit 释放上游新建的 libusb_context(避免泄漏)
 *   3. (*ctx)->libusb_ctx = shared_usb_ctx 共享 PXView 的 context
 *
 * 析构注意:
 *   调用方在 srstd_exit 前必须先把 (*ctx)->libusb_ctx 置 NULL,
 *   否则 srstd_exit 内部 libusb_exit(ctx->libusb_ctx) 会释放共享的 PXView context。
 *   libusb_exit(NULL) 是安全的(libusb 文档明确允许)。
 */
int srstd_init_shared(struct sr_context **ctx, libusb_context *shared_usb_ctx)
{
    int ret;

    if (!ctx || !shared_usb_ctx) {
        return SR_ERR_ARG;
    }

    /* 步骤 1: 调用上游 sr_init 创建 sr_context(内部 libusb_init 创建独立 context) */
    ret = sr_init(ctx);
    if (ret != SR_OK) {
        return ret;
    }

    /*
     * 步骤 2: 释放上游 sr_init 内部创建的 libusb_context
     * (*ctx)->libusb_ctx 此时指向上游新建的 context,非 NULL(HAVE_LIBUSB_1_0 已定义)
     */
    if ((*ctx)->libusb_ctx) {
        libusb_exit((*ctx)->libusb_ctx);
    }

    /* 步骤 3: 替换为 PXView 共享的 libusb_context */
    (*ctx)->libusb_ctx = shared_usb_ctx;

    /*
     * 步骤 4: 驱动注册由上游 section 机制自动完成。
     *
     * sr_init() 内部已调用 sr_drivers_init(ctx),后者在 HAVE_DRIVERS 已
     * 定义的前提下,遍历 __sr_driver_list section(由 driver_list_start.c
     * 和 driver_list_stop.c 定界,各驱动 api.c 通过 SR_REGISTER_DEV_DRIVER
     * 宏登记),将所有驱动指针填入 ctx->driver_list。
     *
     * 因 libsigrokstd 现为 SHARED 库,所有 object 文件均参与链接(不存在
     * 静态库选择性拉入问题),section 边界符号由 start/stop 文件显式定义
     * (不依赖 PE/COFF 自动生成)。故无需在此手动注册。
     */

    return SR_OK;
}
