#ifndef SRSTD_EXPORT_H_
#define SRSTD_EXPORT_H_

/*
 * srstd_export.h - Cross-platform export macro for libsigrokstd shared library.
 *
 * IMPORTANT (Windows behavior):
 *   On Windows, SRSTD_API is intentionally EMPTY. This mirrors upstream
 *   libsigrok's SR_API/SR_PRIV macros, which are also empty on _WIN32
 *   (upstream uses autotools/libtool for DLL export, not __declspec).
 *
 *   The actual DLL export is handled by CMake's WINDOWS_EXPORT_ALL_SYMBOLS=ON,
 *   which auto-generates a .def file from all global symbols and passes
 *   -Wl,--export-all-symbols to the linker.
 *
 *   CRITICAL: If ANY object file in the link contains __declspec(dllexport)
 *   annotations, MinGW's ld SILENTLY DISABLES --export-all-symbols, causing
 *   only the dllexport-annotated symbols to be exported. By keeping SRSTD_API
 *   empty on Windows, we ensure no dllexport annotations exist anywhere in
 *   the build, so --export-all-symbols correctly exports ALL global symbols
 *   (sr_init, sr_exit, sr_driver_list, srstd_glue_*, srstd_pxview_*, etc.).
 *
 * On Linux/macOS, __attribute__((visibility("default"))) is used so that
 * only SRSTD_API-annotated symbols are exported (paired with
 * -fvisibility=hidden in CMakeLists.txt).
 */

#if defined(_WIN32)
  #define SRSTD_API
#else
  #define SRSTD_API __attribute__((visibility("default")))
#endif

#endif /* SRSTD_EXPORT_H_ */
