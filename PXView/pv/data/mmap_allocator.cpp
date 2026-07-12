#include "mmap_allocator.h"
#include <QDebug>
#include <thread>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include "../log.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
// 在 worker 线程路径上避免使用 QDir/QDateTime，改用 std::filesystem + std::chrono
// 生成磁盘缓存文件路径，并保证目录存在。
std::string make_cache_file_path(const QString& disk_dir) {
    namespace fs = std::filesystem;
    fs::path dir_path(disk_dir.toStdString());
    std::error_code ec;
    fs::create_directories(dir_path, ec);  // 不抛异常，已存在则忽略

    // 生成时间戳 yyyyMMddHHmmsszzz
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now_c);
#else
    localtime_r(&now_c, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d%H%M%S");
    oss << std::setfill('0') << std::setw(3) << ms.count();

    fs::path file_path = dir_path / ("pxview_mmap_cache_" + oss.str() + ".dat");
    return file_path.string();
}
} // anonymous namespace

namespace pv {
namespace data {

MmapAllocator::MmapAllocator()
    : _base_ptr(nullptr),
      _total_bytes(0)
#ifdef _WIN32
      , _hMap(nullptr), _hFile(INVALID_HANDLE_VALUE)
#else
      , _fd(-1)
#endif
{
}

MmapAllocator::~MmapAllocator() {
    clear();
}

bool MmapAllocator::configure(bool use_disk_file, const QString& disk_dir, uint64_t total_bytes) {
    std::lock_guard<std::mutex> lock(_mutex);
    clear();

    if (total_bytes == 0) return false;
    _total_bytes = total_bytes;

#ifdef _WIN32
    if (use_disk_file && !disk_dir.isEmpty()) {
        _file_path = QString::fromStdString(make_cache_file_path(disk_dir));

        _hFile = CreateFileA(_file_path.toUtf8().constData(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, // No sharing
                             NULL,
                             CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);

        if (_hFile == INVALID_HANDLE_VALUE) {
            pxv_err("MmapAllocator: Failed to create disk cache file %s, error %lu",
                    _file_path.toUtf8().constData(), GetLastError());
            return false;
        }

        // 将缓存文件标记为稀疏，使零区间不占磁盘空间（配合 LogicSnapshot 跳过 memset + decommit）。
        DWORD bytes_returned = 0;
        if (!DeviceIoControl(_hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
            pxv_warn("MmapAllocator: FSCTL_SET_SPARSE failed (file will be non-sparse), error %lu",
                     GetLastError());
            // 非致命：退化为非稀疏文件，功能正常但磁盘占用更高。
        }
    } else {
        _hFile = INVALID_HANDLE_VALUE; // Page file backed
    }

    _hMap = CreateFileMappingA(_hFile,
                               NULL,
                               PAGE_READWRITE,
                               (DWORD)(_total_bytes >> 32),
                               (DWORD)(_total_bytes & 0xFFFFFFFF),
                               NULL);
                               
    if (!_hMap) {
        pxv_err("MmapAllocator: CreateFileMapping failed, error %lu", GetLastError());
        if (_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(_hFile);
            _hFile = INVALID_HANDLE_VALUE;
        }
        return false;
    }

    _base_ptr = MapViewOfFile(_hMap, FILE_MAP_ALL_ACCESS, 0, 0, _total_bytes);
    if (!_base_ptr) {
        pxv_err("MmapAllocator: MapViewOfFile failed, error %lu", GetLastError());
        CloseHandle(_hMap);
        _hMap = nullptr;
        if (_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(_hFile);
            _hFile = INVALID_HANDLE_VALUE;
        }
        return false;
    }
#else
    if (use_disk_file && !disk_dir.isEmpty()) {
        _file_path = QString::fromStdString(make_cache_file_path(disk_dir));
        _fd = open(_file_path.toUtf8().constData(), O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (_fd < 0) {
            pxv_err("MmapAllocator: Failed to open disk cache file");
            return false;
        }
        if (ftruncate(_fd, _total_bytes) < 0) {
            pxv_err("MmapAllocator: ftruncate failed");
            close(_fd);
            _fd = -1;
            return false;
        }
        _base_ptr = mmap(NULL, _total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    } else {
        _base_ptr = mmap(NULL, _total_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    
    if (_base_ptr == MAP_FAILED) {
        pxv_err("MmapAllocator: mmap failed");
        _base_ptr = nullptr;
        if (_fd >= 0) {
            close(_fd);
            _fd = -1;
        }
        return false;
    }
#endif

    pxv_info("MmapAllocator: Configured successfully, %llu bytes mapped at %p", 
             (unsigned long long)_total_bytes, _base_ptr);
    return true;
}

void* MmapAllocator::get_block_data(int channel, uint64_t block_index, uint64_t max_blocks_per_channel, uint64_t block_size) {
    if (!_base_ptr) return nullptr;
    
    if (max_blocks_per_channel == 0) return nullptr;
    uint64_t wrapped_block_index = block_index % max_blocks_per_channel;
    
    uint64_t global_offset = ((uint64_t)channel * max_blocks_per_channel + wrapped_block_index) * block_size;
    if (global_offset + block_size > _total_bytes) {
        pxv_err("MmapAllocator: Out of bounds access! offset %llu > total %llu", 
                (unsigned long long)(global_offset + block_size), (unsigned long long)_total_bytes);
        return nullptr;
    }
    
    return (uint8_t*)_base_ptr + global_offset;
}

bool MmapAllocator::decommit_block(void* ptr, uint64_t size) {
    if (!ptr || !_base_ptr || !is_mmap_address(ptr)) return false;

#ifdef _WIN32
    // RAM 页：DiscardVirtualMemory 从工作集移除页（不写回脏页），后续读返回零。
    // 适用于 mapped view（VirtualFree MEM_DECOMMIT 对 mapped view 无效）。
    DWORD discard_res = DiscardVirtualMemory(ptr, size);
    if (discard_res != ERROR_SUCCESS) {
        pxv_warn("MmapAllocator: DiscardVirtualMemory failed, error %lu (non-fatal)",
                 discard_res);
    }

    // 磁盘页：对文件区间 punch sparse zero hole，回收磁盘空间。
    if (_hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_offset;
        file_offset.QuadPart = (LONGLONG)((uint8_t*)ptr - (uint8_t*)_base_ptr);
        FILE_ZERO_DATA_INFORMATION fzdi;
        fzdi.FileOffset = file_offset;
        fzdi.BeyondFinalZero.QuadPart = file_offset.QuadPart + (LONGLONG)size;
        DWORD bytes_returned = 0;
        if (!DeviceIoControl(_hFile, FSCTL_SET_ZERO_DATA,
                             &fzdi, sizeof(fzdi),
                             NULL, 0, &bytes_returned, NULL)) {
            pxv_warn("MmapAllocator: FSCTL_SET_ZERO_DATA failed, error %lu (non-fatal)",
                     GetLastError());
        }
    }
    return true;
#else
    // RAM 页：madvise MADV_DONTNEED 释放页，后续读返回零（匿名映射）。
    if (madvise(ptr, size, MADV_DONTNEED) != 0) {
        pxv_warn("MmapAllocator: madvise MADV_DONTNEED failed, errno %d (non-fatal)", errno);
    }

    // 磁盘页：在文件中打洞回收磁盘空间。
    // Linux: fallocate PUNCH_HOLE；macOS: fcntl F_PUNCHHOLE；其他平台跳过
    // (punch hole 仅是磁盘空间回收优化，缺失不影响功能正确性)
    if (_fd >= 0) {
        off_t offset = (off_t)((uint8_t*)ptr - (uint8_t*)_base_ptr);
#ifdef __linux__
        if (fallocate(_fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                      offset, (off_t)size) != 0) {
            pxv_warn("MmapAllocator: fallocate PUNCH_HOLE failed, errno %d (non-fatal)", errno);
        }
#elif defined(__APPLE__)
        struct fpunchhole fpunch;
        fpunch.fp_flags = 0;
        fpunch.fp_offset = offset;
        fpunch.fp_length = (off_t)size;
        if (fcntl(_fd, F_PUNCHHOLE, &fpunch) != 0) {
            pxv_warn("MmapAllocator: fcntl F_PUNCHHOLE failed, errno %d (non-fatal)", errno);
        }
#endif
    }
    return true;
#endif
}

bool MmapAllocator::block_absolute_slot(void* ptr, uint64_t block_size, uint64_t& slot) const {
    if (!ptr || !_base_ptr || block_size == 0) return false;
    if (!is_mmap_address(ptr)) return false;
    slot = (uint64_t)((uint8_t*)ptr - (uint8_t*)_base_ptr) / block_size;
    return true;
}

void MmapAllocator::clear() {
    // Take the file path into a local variable before clearing the member,
    // so the background delete thread never touches object state.
    const QString file_to_delete = _file_path;

#ifdef _WIN32
    if (_base_ptr) {
        UnmapViewOfFile(_base_ptr);
        _base_ptr = nullptr;
    }
    if (_hMap) {
        CloseHandle(_hMap);
        _hMap = nullptr;
    }
    if (_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(_hFile);
        _hFile = INVALID_HANDLE_VALUE;
    }
#else
    if (_base_ptr && _base_ptr != MAP_FAILED) {
        munmap(_base_ptr, _total_bytes);
        _base_ptr = nullptr;
    }
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
#endif
    _total_bytes = 0;
    _file_path.clear();

    // Delete the on-disk cache file in the background. Large cache files
    // (up to several GB / 16 GB by default) can take seconds to tens of
    // seconds to delete, and clear() may be called from LogicSnapshot
    // while holding its mutex, which would otherwise block feed/decode/UI
    // threads. The handles above are already closed synchronously (fast),
    // so the detached thread only needs to remove the file by path.
    if (!file_to_delete.isEmpty()) {
        // 在主线程把 QString 转换为 std::string，避免 detached 线程触碰任何 Qt API。
        const std::string path_to_delete = file_to_delete.toStdString();
        std::thread([path_to_delete]() {
            namespace fs = std::filesystem;
            std::error_code ec;
            // 用带 error_code 的重载避免抛异常。
            if (!fs::exists(path_to_delete, ec)) {
                return;
            }
            if (fs::remove(path_to_delete, ec)) {
                pxv_info("MmapAllocator: Background-deleted cache file %s",
                         path_to_delete.c_str());
            } else {
                pxv_err("MmapAllocator: Failed to background-delete cache file %s (ec=%d: %s)",
                        path_to_delete.c_str(), ec.value(), ec.message().c_str());
            }
        }).detach();
    }
}

} // namespace data
} // namespace pv
