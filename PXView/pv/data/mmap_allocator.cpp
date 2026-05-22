#include "mmap_allocator.h"
#include <QDir>
#include <QDebug>
#include "../log.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

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
        QDir dir(disk_dir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        _file_path = dir.absoluteFilePath("dsview_mmap_cache.dat");
        
        _hFile = CreateFileA(_file_path.toUtf8().constData(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, // No sharing
                             NULL,
                             CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);
                             
        if (_hFile == INVALID_HANDLE_VALUE) {
            dsv_err("MmapAllocator: Failed to create disk cache file %s, error %lu", 
                    _file_path.toUtf8().constData(), GetLastError());
            return false;
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
        dsv_err("MmapAllocator: CreateFileMapping failed, error %lu", GetLastError());
        if (_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(_hFile);
            _hFile = INVALID_HANDLE_VALUE;
        }
        return false;
    }

    _base_ptr = MapViewOfFile(_hMap, FILE_MAP_ALL_ACCESS, 0, 0, _total_bytes);
    if (!_base_ptr) {
        dsv_err("MmapAllocator: MapViewOfFile failed, error %lu", GetLastError());
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
        QDir dir(disk_dir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        _file_path = dir.absoluteFilePath("dsview_mmap_cache.dat");
        _fd = open(_file_path.toUtf8().constData(), O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (_fd < 0) {
            dsv_err("MmapAllocator: Failed to open disk cache file");
            return false;
        }
        if (ftruncate(_fd, _total_bytes) < 0) {
            dsv_err("MmapAllocator: ftruncate failed");
            close(_fd);
            _fd = -1;
            return false;
        }
        _base_ptr = mmap(NULL, _total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    } else {
        _base_ptr = mmap(NULL, _total_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    
    if (_base_ptr == MAP_FAILED) {
        dsv_err("MmapAllocator: mmap failed");
        _base_ptr = nullptr;
        if (_fd >= 0) {
            close(_fd);
            _fd = -1;
        }
        return false;
    }
#endif

    dsv_info("MmapAllocator: Configured successfully, %llu bytes mapped at %p", 
             (unsigned long long)_total_bytes, _base_ptr);
    return true;
}

void* MmapAllocator::get_block_data(int channel, uint64_t block_index, uint64_t max_blocks_per_channel, uint64_t block_size) {
    if (!_base_ptr) return nullptr;
    
    if (block_index >= max_blocks_per_channel) {
        // Technically should not happen if _total_sample_count is correct, but just in case we hit the ceiling.
        // Returning nullptr forces the application to drop data or handle OOM safely.
        dsv_err("MmapAllocator: block_index %llu exceeds max_blocks_per_channel %llu", 
                (unsigned long long)block_index, (unsigned long long)max_blocks_per_channel);
        return nullptr;
    }
    
    uint64_t global_offset = ((uint64_t)channel * max_blocks_per_channel + block_index) * block_size;
    if (global_offset + block_size > _total_bytes) {
        dsv_err("MmapAllocator: Out of bounds access! offset %llu > total %llu", 
                (unsigned long long)(global_offset + block_size), (unsigned long long)_total_bytes);
        return nullptr;
    }
    
    return (uint8_t*)_base_ptr + global_offset;
}

void MmapAllocator::advise_dontneed(void* ptr, uint64_t size) {
    if (!ptr || !is_mmap_address(ptr)) return;
#ifdef _WIN32
    // On Windows, VirtualUnlock hints the memory manager to page out the data to disk if memory is tight.
    VirtualUnlock(ptr, size);
#else
    madvise(ptr, size, MADV_DONTNEED);
#endif
}


void MmapAllocator::clear() {
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
        if (!_file_path.isEmpty()) {
            QFile::remove(_file_path);
        }
    }
#else
    if (_base_ptr && _base_ptr != MAP_FAILED) {
        munmap(_base_ptr, _total_bytes);
        _base_ptr = nullptr;
    }
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
        if (!_file_path.isEmpty()) {
            QFile::remove(_file_path);
        }
    }
#endif
    _total_bytes = 0;
    _file_path.clear();
}

} // namespace data
} // namespace pv
