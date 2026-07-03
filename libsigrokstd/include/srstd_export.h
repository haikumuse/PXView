#ifndef SRSTD_EXPORT_H_
#define SRSTD_EXPORT_H_

/*
 * srstd_export.h - Cross-platform export macro for libsigrokstd shared library.
 *
 * SRSTD_API marks functions exported from libsigrokstd.dll/.so/.dylib.
 * When compiling libsigrokstd itself, SRSTD_BUILDING_DLL must be defined
 * (done by CMake target_compile_definitions) so SRSTD_API expands to
 * __declspec(dllexport) on Windows. When consuming code (PXView) includes
 * this header without SRSTD_BUILDING_DLL, SRSTD_API expands to
 * __declspec(dllimport) on Windows. On Linux/macOS,
 * __attribute__((visibility("default"))) is used regardless.
 */

#if defined(_WIN32)
  #if defined(SRSTD_BUILDING_DLL)
    #define SRSTD_API __declspec(dllexport)
  #else
    #define SRSTD_API __declspec(dllimport)
  #endif
#else
  #define SRSTD_API __attribute__((visibility("default")))
#endif

#endif /* SRSTD_EXPORT_H_ */
