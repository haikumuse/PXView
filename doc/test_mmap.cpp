#include <windows.h>
#include <iostream>

int main() {
    uint64_t size = 16ULL * 1024 * 1024 * 1024; // 16GB
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, size >> 32, size & 0xFFFFFFFF, NULL);
    if (!hMap) {
        std::cout << "CreateFileMapping failed: " << GetLastError() << std::endl;
        return 1;
    }
    void* ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!ptr) {
        std::cout << "MapViewOfFile failed: " << GetLastError() << std::endl;
        return 1;
    }
    std::cout << "Successfully mapped 16GB!" << std::endl;
    return 0;
}
