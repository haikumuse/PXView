#ifndef PXVIEW_PV_DATA_MMAP_ALLOCATOR_H
#define PXVIEW_PV_DATA_MMAP_ALLOCATOR_H

#include <string>
#include <cstdint>
#include <mutex>
#include <thread>
#include <QString>

namespace pv {
namespace data {

class MmapAllocator {
public:
    MmapAllocator();
    ~MmapAllocator();

    bool configure(bool use_disk_file, const QString& disk_dir, uint64_t total_bytes);
    void* get_block_data(int channel, uint64_t block_index, uint64_t max_blocks_per_channel, uint64_t block_size);

    // 归还指定块区间的物理页给 OS（不释放虚拟映射）。
    // - 匿名 mmap (RAM 模式)：decommit 后页读回零。
    // - 文件 mmap (磁盘模式)：decommit RAM 页 + 对文件区间 punch sparse zero hole，回收磁盘空间。
    // 调用方需保证该块已不再被读（lbp 已置 NULL）。
    bool decommit_block(void* ptr, uint64_t size);

    // 由 mmap 地址反推绝对槽位序号 abs_slot = (ptr - base) / block_size。
    // 用于 LogicSnapshot 的 written 位图清位。失败（ptr 不在 mmap 区间）返回 false。
    bool block_absolute_slot(void* ptr, uint64_t block_size, uint64_t& slot) const;

    void clear();

    bool is_mmap_address(void* ptr) const {
        if (!_base_ptr) return false;
        return (uint8_t*)ptr >= (uint8_t*)_base_ptr && 
               (uint8_t*)ptr < ((uint8_t*)_base_ptr + _total_bytes);
    }
    
    uint64_t get_total_bytes() const { return _total_bytes; }

private:
    void* _base_ptr;
    uint64_t _total_bytes;
    QString _file_path;
#ifdef _WIN32
    void* _hMap;
    void* _hFile;
#else
    int _fd;
#endif
    std::mutex _mutex;
};

} // namespace data
} // namespace pv

#endif // PXVIEW_PV_DATA_MMAP_ALLOCATOR_H
