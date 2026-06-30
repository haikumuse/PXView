#!/usr/bin/env python3
"""
驱动迁移自动化脚本 - 用于将标准 sigrok 驱动迁移到 PXView 项目

用法: python scripts/migrate_driver.py <driver_name>

功能:
1. 从标准sigrok复制驱动源码
2. 替换头文件include
3. 生成wrapper函数模板
4. 生成hwdriver.c注册代码
5. 生成CMakeLists.txt条目
"""

import os
import sys
import shutil
import re
from pathlib import Path

# 路径配置
OLD_SIGROK_PATH = Path(r"c:\Users\admin\Downloads\old\libsigrok\src\hardware")
NEW_SIGROK_PATH = Path(r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware")
COMPAT_PATH = NEW_SIGROK_PATH / "compat"
HWDIVER_PATH = Path(r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hwdriver.c")
CMAKELISTS_PATH = Path(r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\CMakeLists.txt")


def normalize_driver_name(driver_name: str) -> str:
    """将驱动名规范化为下划线格式 (用于宏和变量名)"""
    return driver_name.replace("-", "_").replace(".", "_")


def get_driver_macro_name(driver_name: str) -> str:
    """生成驱动宏名称，如 HAVE_DRIVER_FX2LAFW"""
    normalized = normalize_driver_name(driver_name)
    return f"HAVE_DRIVER_{normalized.upper()}"


def get_driver_option_name(driver_name: str) -> str:
    """生成 CMake option 名称，如 ENABLE_DRIVER_FX2LAFW"""
    normalized = normalize_driver_name(driver_name)
    return f"ENABLE_DRIVER_{normalized.upper()}"


def get_driver_info_name(driver_name: str) -> str:
    """生成 driver_info 结构体名称"""
    normalized = normalize_driver_name(driver_name)
    return f"{normalized}_driver_info"


def get_driver_longname(driver_name: str) -> str:
    """根据驱动名生成描述性长名称"""
    # 常见驱动名映射
    known_names = {
        "fx2lafw": "fx2lafw (generic driver for FX2 based LAs)",
        "saleae-logic16": "Saleae Logic16",
        "raspberrypi-pico": "Raspberry Pi Pico",
    }
    return known_names.get(driver_name, f"{driver_name} (sigrok compat driver)")


def check_source_driver_exists(driver_name: str) -> bool:
    """检查源驱动目录是否存在"""
    source_path = OLD_SIGROK_PATH / driver_name
    return source_path.exists() and source_path.is_dir()


def copy_driver_source(driver_name: str) -> bool:
    """复制驱动源码到目标目录"""
    source_path = OLD_SIGROK_PATH / driver_name
    target_path = NEW_SIGROK_PATH / driver_name
    
    if not source_path.exists():
        print(f"❌ 错误: 源驱动目录不存在: {source_path}")
        return False
    
    if target_path.exists():
        print(f"⚠️  目标目录已存在: {target_path}")
        print(f"   将覆盖现有文件...")
        # 清空目标目录
        shutil.rmtree(target_path)
    
    # 复制目录
    shutil.copytree(source_path, target_path)
    print(f"✅ 已复制驱动源码: {source_path} -> {target_path}")
    
    # 列出复制的文件
    copied_files = list(target_path.glob("*"))
    print(f"   复制的文件: {[f.name for f in copied_files]}")
    
    return True


def replace_includes_in_file(file_path: Path) -> bool:
    """替换单个文件中的头文件 include"""
    try:
        content = file_path.read_text(encoding='utf-8', errors='replace')
    except Exception as e:
        print(f"❌ 无法读取文件 {file_path}: {e}")
        return False
    
    original = content
    
    # 替换模式
    replacements = [
        # 主替换: <libsigrok/libsigrok.h> -> compat.h
        (r'#include\s*<libsigrok/libsigrok\.h>',
         '#include "hardware/compat/compat.h"'),
        # 替换 libsigrok-internal.h
        (r'#include\s*"libsigrok-internal\.h"',
         '#include "hardware/compat/compat.h"'),
        # 替换 config.h (保留本地 config.h)
        # 注意: 需要保留 #include <config.h> 或 "config.h"
    ]
    
    for pattern, replacement in replacements:
        content = re.sub(pattern, replacement, content)
    
    if content != original:
        try:
            file_path.write_text(content, encoding='utf-8')
            print(f"   ✅ 已替换头文件: {file_path.name}")
            return True
        except Exception as e:
            print(f"❌ 无法写入文件 {file_path}: {e}")
            return False
    
    return True


def replace_includes_in_driver(driver_name: str) -> bool:
    """替换驱动目录中所有源文件的头文件 include"""
    target_path = NEW_SIGROK_PATH / driver_name
    
    # 查找所有 .c 和 .h 文件
    source_files = list(target_path.glob("*.c")) + list(target_path.glob("*.h"))
    
    print(f"\n📝 替换头文件 include...")
    for file_path in source_files:
        replace_includes_in_file(file_path)
    
    return True


def analyze_driver_api(driver_name: str) -> dict:
    """分析驱动 api.c 文件，提取关键信息"""
    api_path = NEW_SIGROK_PATH / driver_name / "api.c"
    
    if not api_path.exists():
        print(f"⚠️  api.c 文件不存在，将生成默认模板")
        return None
    
    try:
        content = api_path.read_text(encoding='utf-8', errors='replace')
    except Exception as e:
        print(f"❌ 无法读取 api.c: {e}")
        return None
    
    info = {
        'driver_name': driver_name,
        'normalized_name': normalize_driver_name(driver_name),
        'has_config_get': bool(re.search(r'static\s+int\s+config_get\s*\(', content)),
        'has_config_set': bool(re.search(r'static\s+int\s+config_set\s*\(', content)),
        'has_config_list': bool(re.search(r'static\s+int\s+config_list\s*\(', content)),
        'has_dev_open': bool(re.search(r'static\s+int\s+dev_open\s*\(', content)),
        'has_dev_close': bool(re.search(r'static\s+int\s+dev_close\s*\(', content)),
        'has_dev_acquisition_start': bool(re.search(r'static\s+int\s+dev_acquisition_start\s*\(', content)),
        'has_dev_acquisition_stop': bool(re.search(r'static\s+int\s+dev_acquisition_stop\s*\(', content)),
        'has_dev_clear': bool(re.search(r'static\s+int\s+dev_clear\s*\(', content)),
        'has_scan': bool(re.search(r'static\s+GSList\s+\*\s*scan\s*\(', content)),
        # 检查是否有特定的 acquisition start/stop 函数名
        'acquisition_start_name': None,
        'acquisition_stop_name': None,
    }
    
    # 尝找具体的 acquisition 函数名
    start_match = re.search(r'static\s+int\s+(\w+)\s*\([^)]*sr_dev_inst[^)]*\).*acquisition.*start', content, re.IGNORECASE)
    if start_match:
        info['acquisition_start_name'] = start_match.group(1)
    
    stop_match = re.search(r'static\s+int\s+(\w+)\s*\([^)]*sr_dev_inst[^)]*\).*acquisition.*stop', content, re.IGNORECASE)
    if stop_match:
        info['acquisition_stop_name'] = stop_match.group(1)
    
    return info


def generate_wrapper_functions(driver_name: str, driver_info: dict = None) -> str:
    """生成 wrapper 函数模板代码"""
    normalized = normalize_driver_name(driver_name)
    
    # 如果没有分析结果，使用默认模板
    if driver_info is None:
        driver_info = {
            'normalized_name': normalized,
            'has_config_get': True,
            'has_config_set': True,
            'has_config_list': True,
            'has_dev_open': True,
            'has_dev_close': True,
            'has_dev_acquisition_start': True,
            'has_dev_acquisition_stop': True,
            'has_dev_clear': True,
            'has_scan': True,
            'acquisition_start_name': f'{normalized}_start_acquisition',
            'acquisition_stop_name': 'dev_acquisition_stop',
        }
    
    # 使用 {{ 和 }} 转义字面大括号，{normalized} 用于变量插值
    code = f'''
/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *{normalized}_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver {normalized}_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int {normalized}_compat_init(struct sr_context *sr_ctx)
{{'''
    code += f'''
    {normalized}_drv_ptr = &{normalized}_driver_info;
    return std_init({normalized}_drv_ptr, sr_ctx);
'''

    code += f'''}}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int {normalized}_compat_cleanup(void)
{{'''
    code += f'''
    /* Call dev_clear to free per-device resources before cleanup */
'''
    
    if driver_info.get('has_dev_clear'):
        code += f'    dev_clear({normalized}_drv_ptr);\n'
    else:
        code += f'    std_dev_clear_compat({normalized}_drv_ptr);\n'
    
    code += f'''    return std_cleanup({normalized}_drv_ptr);
'''
    code += f'''}}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *{normalized}_compat_scan(GSList *options)
{{'''
    code += f'''
    return scan({normalized}_drv_ptr, options);
'''
    code += f'''}}

'''
    
    # config_get wrapper
    if driver_info.get('has_config_get'):
        code += f'''/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int {normalized}_compat_config_get(int id, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel *ch,
    const struct sr_channel_group *cg)
{{'''
        code += f'''
    (void)ch;
    return config_get((uint32_t)id, data, sdi, cg);
'''
        code += f'''}}

'''
    
    # config_set wrapper
    if driver_info.get('has_config_set'):
        code += f'''/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int {normalized}_compat_config_set(int id, GVariant *data,
    struct sr_dev_inst *sdi, struct sr_channel *ch,
    struct sr_channel_group *cg)
{{'''
        code += f'''
    (void)ch;
    return config_set((uint32_t)id, data, sdi, cg);
'''
        code += f'''}}

'''
    
    # config_list wrapper
    if driver_info.get('has_config_list'):
        code += f'''/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int {normalized}_compat_config_list(int info_id, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{{'''
        code += f'''
    return config_list((uint32_t)info_id, data, sdi, cg);
'''
        code += f'''}}

'''
    
    # acquisition_start wrapper
    start_func = driver_info.get('acquisition_start_name') or 'dev_acquisition_start'
    code += f'''/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int {normalized}_compat_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{{'''
    code += f'''
    (void)cb_data;
    return {start_func}(sdi);
'''
    code += f'''}}

'''
    
    # acquisition_stop wrapper
    stop_func = driver_info.get('acquisition_stop_name') or 'dev_acquisition_stop'
    code += f'''/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int {normalized}_compat_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{{'''
    code += f'''
    (void)cb_data;
    return {stop_func}((struct sr_dev_inst *)sdi);
'''
    code += f'''}}

'''
    
    return code


def generate_driver_info_struct(driver_name: str, driver_info: dict = None) -> str:
    """生成 driver_info 结构体定义"""
    normalized = normalize_driver_name(driver_name)
    longname = get_driver_longname(driver_name)
    
    # 如果没有分析结果，使用默认值
    if driver_info is None:
        driver_info = {
            'normalized_name': normalized,
            'has_dev_open': True,
            'has_dev_close': True,
        }
    
    open_func = 'dev_open' if driver_info.get('has_dev_open') else 'NULL'
    close_func = 'dev_close' if driver_info.get('has_dev_close') else 'NULL'
    
    # 使用 {{ 和 }} 转义结构体的大括号
    code = f'''/* PXView-compatible driver info struct */
struct sr_dev_driver {normalized}_driver_info = {{
    .name = "{driver_name}",
    .longname = "{longname}",
    .api_version = 1,
    .driver_type = DRIVER_TYPE_HARDWARE,
    .init = {normalized}_compat_init,
    .cleanup = {normalized}_compat_cleanup,
    .scan = {normalized}_compat_scan,
    .dev_mode_list = compat_dev_mode_list_default,
    .config_get = {normalized}_compat_config_get,
    .config_set = {normalized}_compat_config_set,
    .config_list = {normalized}_compat_config_list,
    .dev_open = {open_func},
    .dev_close = {close_func},
    .dev_destroy = compat_dev_destroy_default,
    .dev_status_get = compat_dev_status_get_default,
    .dev_acquisition_start = {normalized}_compat_acquisition_start,
    .dev_acquisition_stop = {normalized}_compat_acquisition_stop,
    .priv = NULL,
}};
'''
    
    return code


def generate_hwdriver_registration(driver_name: str) -> str:
    """生成 hwdriver.c 注册代码片段"""
    normalized = normalize_driver_name(driver_name)
    macro_name = get_driver_macro_name(driver_name)
    info_name = get_driver_info_name(driver_name)
    
    extern_code = f'''
#ifdef {macro_name}
extern SR_PRIV struct sr_dev_driver {info_name};
#endif
'''
    
    list_entry = f'''
#ifdef {macro_name}
    &{info_name},
#endif
'''
    
    return extern_code, list_entry


def generate_cmake_entries(driver_name: str) -> str:
    """生成 CMakeLists.txt 条目片段"""
    normalized = normalize_driver_name(driver_name)
    option_name = get_driver_option_name(driver_name)
    macro_name = get_driver_macro_name(driver_name)
    
    # Option 定义
    option_code = f'''option({option_name} "Enable {driver_name} driver" OFF)
'''
    
    # Definition 条目
    definition_code = f'''    if({option_name})
        add_definitions(-D{macro_name})
    endif()
'''
    
    # Source 文件条目
    source_code = f'''    if({option_name})
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/{driver_name}/api.c
            libsigrok/hardware/{driver_name}/protocol.c
        )
    endif()
'''
    
    return option_code, definition_code, source_code


def print_migration_instructions(driver_name: str):
    """打印迁移后续步骤说明"""
    normalized = normalize_driver_name(driver_name)
    macro_name = get_driver_macro_name(driver_name)
    option_name = get_driver_option_name(driver_name)
    
    print(f"\n{'='*60}")
    print("📋 后续手动操作步骤:")
    print(f"{'='*60}")
    
    print(f"\n1️⃣  修改 hwdriver.c ({HWDIVER_PATH})")
    print(f"   - 在 extern 声明区域添加:")
    print(f"     #ifdef {macro_name}")
    print(f"     extern SR_PRIV struct sr_dev_driver {normalized}_driver_info;")
    print(f"     #endif")
    print(f"   - 在 drivers_list[] 数组添加:")
    print(f"     #ifdef {macro_name}")
    print(f"     &{normalized}_driver_info,")
    print(f"     #endif")
    
    print(f"\n2️⃣  修改 CMakeLists.txt ({CMAKELISTS_PATH})")
    print(f"   - 在驱动 option 区域添加:")
    print(f"     option({option_name} \"Enable {driver_name} driver\" OFF)")
    print(f"   - 在 ENABLE_COMPAT_DRIVERS 块内添加定义:")
    print(f"     if({option_name})")
    print(f"         add_definitions(-D{macro_name})")
    print(f"     endif()")
    print(f"   - 在源文件列表块内添加:")
    print(f"     if({option_name})")
    print(f"         list(APPEND libsigrok_SOURCES")
    print(f"             libsigrok/hardware/{driver_name}/api.c")
    print(f"             libsigrok/hardware/{driver_name}/protocol.c")
    print(f"         )")
    print(f"     endif()")
    
    print(f"\n3️⃣  完善 api.c 包装函数")
    print(f"   - 检查生成的 wrapper 函数是否正确引用原始函数名")
    print(f"   - 如果驱动有特殊的函数命名，需要手动调整 wrapper 中的函数调用")
    
    print(f"\n4️⃣  检查 protocol.c")
    print(f"   - 确保 protocol.c 中的函数可以被 api.c 正确调用")
    print(f"   - 如果 protocol.c 中有 API 函数，也需要添加 compat.h include")
    
    print(f"\n5️⃣  编译测试")
    print(f"   - 在 CMake 配置时启用驱动:")
    print(f"     cmake -DENABLE_COMPAT_DRIVERS=ON -D{option_name}=ON ..")
    print(f"   - 或在 build 目录重新配置:")
    print(f"     cmake -D{option_name}=ON .")
    print(f"   - 编译: ninja")
    
    print(f"\n{'='*60}")


def write_wrapper_code_to_api(driver_name: str):
    """将 wrapper 函数代码追加到 api.c 文件末尾"""
    api_path = NEW_SIGROK_PATH / driver_name / "api.c"
    
    if not api_path.exists():
        print(f"❌ api.c 文件不存在，无法追加 wrapper 代码")
        return False
    
    # 分析驱动
    driver_info = analyze_driver_api(driver_name)
    
    # 生成 wrapper 函数
    wrapper_code = generate_wrapper_functions(driver_name, driver_info)
    driver_info_struct = generate_driver_info_struct(driver_name, driver_info)
    
    # 读取现有内容
    try:
        content = api_path.read_text(encoding='utf-8', errors='replace')
    except Exception as e:
        print(f"❌ 无法读取 api.c: {e}")
        return False
    
    # 检查是否已经有 wrapper 代码
    if f"{normalize_driver_name(driver_name)}_compat_init" in content:
        print(f"⚠️  api.c 已包含 compat wrapper 代码，跳过追加")
        return True
    
    # 检查是否已有 driver_info 结构体
    info_name = get_driver_info_name(driver_name)
    if info_name in content:
        print(f"⚠️  api.c 已包含 {info_name} 结构体定义")
        print(f"   请手动替换为 compat 版本")
        # 不追加，只打印生成的代码供参考
        print(f"\n生成的 wrapper 代码参考:")
        print(wrapper_code)
        print(driver_info_struct)
        return True
    
    # 找到文件末尾，追加 wrapper 和 driver_info
    full_code = wrapper_code + driver_info_struct
    
    # 尝找合适的插入位置 - 在最后一个函数定义之后
    # 或者简单追加到文件末尾
    new_content = content + "\n" + full_code
    
    try:
        api_path.write_text(new_content, encoding='utf-8')
        print(f"✅ 已将 wrapper 代码追加到 api.c")
        return True
    except Exception as e:
        print(f"❌ 无法写入 api.c: {e}")
        print(f"\n生成的 wrapper 代码 (请手动添加到 api.c 末尾):")
        print(wrapper_code)
        print(driver_info_struct)
        return False


def generate_patch_files(driver_name: str):
    """生成需要手动合并的代码片段文件"""
    patches_dir = Path(r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\scripts\patches")
    patches_dir.mkdir(parents=True, exist_ok=True)
    
    normalized = normalize_driver_name(driver_name)
    
    # hwdriver.c 补丁
    extern_code, list_entry = generate_hwdriver_registration(driver_name)
    hwdriver_patch = patches_dir / f"{driver_name}_hwdriver.patch"
    hwdriver_patch.write_text(f'''/* hwdriver.c extern 声明 - 放在现有 extern 块之后 */
{extern_code}

/* hwdriver.c drivers_list[] 条目 - 放在现有条目之后 */
{list_entry}
''', encoding='utf-8')
    print(f"✅ 生成 hwdriver.c 补丁: {hwdriver_patch}")
    
    # CMakeLists.txt 补丁
    option_code, definition_code, source_code = generate_cmake_entries(driver_name)
    cmake_patch = patches_dir / f"{driver_name}_cmake.patch"
    cmake_patch.write_text(f'''/* CMakeLists.txt option 定义 - 放在现有 option 定义之后 */
{option_code}

/* CMakeLists.txt add_definitions - 放在 ENABLE_COMPAT_DRIVERS 块内 */
{definition_code}

/* CMakeLists.txt 源文件 - 放在现有源文件块内 */
{source_code}
''', encoding='utf-8')
    print(f"✅ 生成 CMakeLists.txt 补丁: {cmake_patch}")
    
    # Wrapper 代码参考文件
    driver_info = analyze_driver_api(driver_name)
    wrapper_code = generate_wrapper_functions(driver_name, driver_info)
    driver_info_struct = generate_driver_info_struct(driver_name, driver_info)
    wrapper_patch = patches_dir / f"{driver_name}_wrapper.c"
    wrapper_patch.write_text(wrapper_code + driver_info_struct, encoding='utf-8')
    print(f"✅ 生成 wrapper 代码参考: {wrapper_patch}")


def main():
    """主函数"""
    if len(sys.argv) < 2:
        print("用法: python scripts/migrate_driver.py <driver_name>")
        print("示例: python scripts/migrate_driver.py fx2lafw")
        print("      python scripts/migrate_driver.py saleae-logic16")
        sys.exit(1)
    
    driver_name = sys.argv[1].strip()
    
    print(f"\n{'='*60}")
    print(f"🚀 驱动迁移脚本 - {driver_name}")
    print(f"{'='*60}")
    
    # 1. 检查源驱动目录
    print(f"\n📍 检查源驱动目录...")
    if not check_source_driver_exists(driver_name):
        print(f"❌ 源驱动目录不存在: {OLD_SIGROK_PATH / driver_name}")
        print(f"   请确认 old 目录路径是否正确")
        sys.exit(1)
    print(f"✅ 源驱动目录存在")
    
    # 2. 复制驱动源码
    print(f"\n📍 复制驱动源码...")
    if not copy_driver_source(driver_name):
        print(f"❌ 复制失败")
        sys.exit(1)
    
    # 3. 替换头文件 include
    print(f"\n📍 替换头文件 include...")
    replace_includes_in_driver(driver_name)
    
    # 4. 分析驱动 API
    print(f"\n📍 分析驱动 API...")
    driver_info = analyze_driver_api(driver_name)
    if driver_info:
        print(f"   检测到函数:")
        for key, value in driver_info.items():
            if key.startswith('has_') and value:
                print(f"     - {key}")
    
    # 5. 生成 wrapper 代码并追加到 api.c
    print(f"\n📍 生成 wrapper 代码...")
    write_wrapper_code_to_api(driver_name)
    
    # 6. 生成补丁文件
    print(f"\n📍 生成补丁参考文件...")
    generate_patch_files(driver_name)
    
    # 7. 打印后续步骤
    print_migration_instructions(driver_name)
    
    print(f"\n✅ 驱动迁移基础工作完成!")
    print(f"   请按照上述步骤手动完成剩余集成工作")


if __name__ == "__main__":
    main()