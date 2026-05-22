#include <windows.h>
#include <psapi.h>
int main() { PROCESS_MEMORY_COUNTERS pmc; K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)); return 0; }
