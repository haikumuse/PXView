/*
 * srstd_init_shared.c - libusb_context 共享包装函数
 *
 * 让上游 libsigrokstd 库共享 PXView 已创建的 libusb_context,
 * 避免两库各自 libusb_init() 导致的设备发现/热插拔隔离。
 *
 * 本文件编译时也通过 -include srstd_rename.h 注入宏,
 * 所以源码中写 sr_init/sr_exit/struct sr_context 等原符号名,
 * 预处理后会变成 srstd_init/srstd_exit/struct srstd_context。
 * srstd_init_shared 函数名本身不匹配 sr_* 模式,不会被重命名。
 * SR_OK/SR_ERR_ARG 是 enum 值(非 enum 标签),也不会被重命名。
 * libusb_* 不在重命名范围内。
 */

#include "srstd.h"              /* 包含 libsigrok.h + srstd_rename.h */
#include "libsigrok-internal.h" /* struct sr_context 完整定义(含 libusb_ctx 字段) */
#include <libusb.h>

/*
 * srstd_get_all_drivers() 定义在 src/driver_registry.c(由
 * tools/migrate_drivers_to_srstd.py 自动生成)。
 * 返回一个 NULL 终止的 driver_info 指针数组,包含所有已迁移的硬件驱动。
 *
 * 该函数名以 srstd_ 开头(非 sr_),因此不会被 srstd_rename.h 重命名。
 * 返回的数组元素类型为 struct sr_dev_driver *,在本文件中(经
 * srstd_rename.h 宏展开)变为 struct srstd_dev_driver *,与
 * driver_registry.c(同样经 srstd_rename.h 编译)中的类型一致。
 */
extern struct sr_dev_driver **srstd_get_all_drivers(void);

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
     * 步骤 4: 批量注册所有已迁移驱动到 driver_list。
     *
     * driver_registry.c(由 tools/migrate_drivers_to_srstd.py 自动生成)
     * 提供 srstd_get_all_drivers(),返回一个 NULL 终止的 driver_info
     * 指针数组,涵盖所有已迁移到 libsigrokstd/src/hardware/ 下的驱动。
     *
     * 上游 libsigrok 用 SR_REGISTER_DEV_DRIVER 宏 + ELF section 机制自动
     * 注册驱动。但 libsigrokstd 编译为 Windows 静态库,PE/COFF 格式下:
     *   1) 静态库中只有被引用的 object 文件才会被链接器拉入;
     *   2) PE/COFF 不自动生成 __start_/__stop_ section 边界符号;
     *   3) section 内顺序依赖链接顺序,不可靠。
     * 因此 HAVE_DRIVERS 未定义,section 机制禁用,改为在 sr_init() 之后
     * 调用 srstd_get_all_drivers() 取得驱动数组,计数后分配新的
     * driver_list 并把所有指针拷贝进去。
     *
     * driver_registry.c 中对每个 <name>_driver_info 的 extern 引用会
     * 强制链接器将对应 api.o 从静态库中拉入(否则其代码和数据都会被丢弃)。
     */
    {
        struct sr_dev_driver **migrated = srstd_get_all_drivers();
        if (migrated) {
            int count = 0;
            struct sr_dev_driver **old_list;
            struct sr_dev_driver **new_list;
            int i;

            while (migrated[count])
                count++;

            old_list = (*ctx)->driver_list;
            /* old_list 来自 sr_drivers_init() 的 g_array_free(array, FALSE),
             * HAVE_DRIVERS 未定义时为仅含 NULL 终止符的空数组(1 个槽位)。
             * 重新分配为 count+1 个槽位: count 个驱动 + NULL 终止符。 */
            new_list = g_malloc((count + 1) * sizeof(struct sr_dev_driver *));
            for (i = 0; i < count; i++)
                new_list[i] = migrated[i];
            new_list[count] = NULL;
            (*ctx)->driver_list = new_list;
            g_free(old_list);
        }
    }

    return SR_OK;
}
