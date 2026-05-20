DSView-main_2026_4_27cppnb/build on  cppverdebug [$✘!⇡]
❯ cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install-qt6.dir -DCMAKE_PREFIX_PATH=/mingw64 -DQT_VERSION_FORCE=6
CMake Deprecation Warning at CMakeLists.txt:22 (cmake_minimum_required):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


-- The C compiler identification is GNU 15.2.0
-- The CXX compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: D:/msys64/mingw64/bin/cc.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: D:/msys64/mingw64/bin/c++.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Found PkgConfig: D:/msys64/mingw64/bin/pkg-config.exe (found version "2.5.1")
-- Checking for one of the modules 'glib-2.0'
----- glib-2.0:
--       includes:D:/msys64/mingw64/include/glib-2.0D:/msys64/mingw64/lib/glib-2.0/includeD:/msys64/mingw64/include
--       libraries:D:/msys64/mingw64/lib/libglib-2.0.*
CMake Warning (dev) at CMakeLists.txt:121 (find_package):
  Policy CMP0148 is not set: The FindPythonInterp and FindPythonLibs modules
  are removed.  Run "cmake --help-policy CMP0148" for policy details.  Use
  the cmake_policy command to set the policy and suppress this warning.

This warning is for project developers.  Use -Wno-dev to suppress it.

----- 2python(3.14.4):
--       includes:D:/msys64/mingw64/include/python3.14
--       libraries:D:/msys64/mingw64/lib/libpython3.14.dll.a
----- FFTW:
--       includes:D:/msys64/mingw64/include
--       libraries:D:/msys64/mingw64/lib/libfftw3.dll.a
----- libusb-1.0:
--       includes:D:/msys64/mingw64/include/libusb-1.0
--       libraries:D:/msys64/mingw64/lib/libusb-1.0.dll.a
----- zlib:
--       includes:D:/msys64/mingw64/include
--       libraries:D:/msys64/mingw64/lib/libz.dll.a
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
-- Found Threads: TRUE
-- Performing Test HAVE_STDATOMIC
-- Performing Test HAVE_STDATOMIC - Success
-- Found WrapAtomic: TRUE
-- Found WrapVulkanHeaders: D:/msys64/mingw64/include
CMake Warning at D:/msys64/mingw64/lib/cmake/Qt6/QtPublicDependencyHelpers.cmake:339 (message):
  This project is using headers of the GuiPrivate module and will therefore
  be tied to this specific Qt module build version.  Running this project
  against other versions of the Qt modules may crash at any arbitrary point.
  This is not a bug, but a result of using Qt internals.  You have been
  warned!

  You can disable this warning by setting QT_NO_PRIVATE_MODULE_WARNING to ON.
Call Stack (most recent call first):
  D:/msys64/mingw64/lib/cmake/Qt6GuiPrivate/Qt6GuiPrivateConfig.cmake:48 (_qt_internal_show_private_module_warning)
  D:/msys64/mingw64/lib/cmake/Qt6/Qt6Config.cmake:246 (find_package)
  CMakeLists.txt:180 (find_package)


----- Qt6:
--       includes:D:/msys64/mingw64/include/qt6/QtCoreD:/msys64/mingw64/include/qt6
CMake Warning (dev) at CMakeLists.txt:196 (find_package):
  Policy CMP0167 is not set: The FindBoost module is removed.  Run "cmake
  --help-policy CMP0167" for policy details.  Use the cmake_policy command to
  set the policy and suppress this warning.

This warning is for project developers.  Use -Wno-dev to suppress it.

----- boost:
--       includes:D:/msys64/mingw64/include
-- Output dir: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build.dir
-- Language files copied to: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build.dir/lang
-- Decoder files copied to: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build.dir/decoders
-- C decoder output directory: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build.dir/decoders/c_decoders
-- Configuring done (12.7s)
-- Generating done (1.2s)
CMake Warning:
  Manually-specified variables were not used by the project:

    QT_VERSION_FORCE


-- Build files have been written to: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build

DSView-main_2026_4_27cppnb/build on  cppverdebug [$!⇡] via △ v4.3.2 took 14s
❯ ninja install
[2/515] Generating PXView/pv/moc_sessionmanager.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sessionmanager.h: note: No relevant classes found. No output generated.
[3/515] Generating PXView/pv/moc_log.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/log.h:43:1: note: No relevant classes found. No output generated.
[5/515] Generating PXView/pv/interface/moc_icontextaware.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/interface/icontextaware.h: note: No relevant classes found. No output generated.
[31/515] Generating PXView/pv/moc_sigsession.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.h: note: No relevant classes found. No output generated.
[65/515] Generating PXView/pv/data/moc_datasource.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/datasource.h: note: No relevant classes found. No output generated.
[66/515] Generating PXView/pv/data/moc_decodermodel.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decodermodel.h: note: No relevant classes found. No output generated.
[68/515] Generating PXView/pv/moc_appcontrol.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/appcontrol.h: note: No relevant classes found. No output generated.
[70/515] Generating PXView/pv/config/moc_appconfig.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/config/appconfig.h: note: No relevant classes found. No output generated.
[73/515] Generating PXView/pv/data/decode/moc_annotationrestable.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/annotationrestable.h: note: No relevant classes found. No output generated.
[74/515] Generating PXView/pv/data/decode/moc_decoderstatus.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/decoderstatus.h: note: No relevant classes found. No output generated.
[76/515] Generating PXView/pv/ui/moc_msgbox.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/msgbox.h: note: No relevant classes found. No output generated.
[78/515] Generating PXView/pv/moc_ZipMaker.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ZipMaker.h: note: No relevant classes found. No output generated.
[83/515] Generating PXView/pv/moc_dsvdef.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dsvdef.h: note: No relevant classes found. No output generated.
[96/515] Generating PXView/pv/ui/moc_fn.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/fn.h:55:1: note: No relevant classes found. No output generated.
[97/515] Generating PXView/pv/ui/moc_iconcache.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/iconcache.h: note: No relevant classes found. No output generated.
[98/515] Generating PXView/pv/data/moc_sessiondocument.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessiondocument.h: note: No relevant classes found. No output generated.
[99/515] Generating PXView/pv/utility/moc_array.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/utility/array.h:36:1: note: No relevant classes found. No output generated.
[100/515] Generating PXView/pv/moc_tabcontext.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/tabcontext.h: note: No relevant classes found. No output generated.
[101/515] Generating PXView/pv/utility/moc_encoding.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/utility/encoding.h:35:1: note: No relevant classes found. No output generated.
[102/515] Generating PXView/pv/data/moc_sessionsnapshot.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessionsnapshot.h: note: No relevant classes found. No output generated.
[103/515] Generating PXView/pv/utility/moc_path.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/utility/path.h:37:1: note: No relevant classes found. No output generated.
[106/515] Generating PXView/pv/moc_deviceagent.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.h:239:1: note: No relevant classes found. No output generated.
[108/515] Generating PXView/pv/moc_winnativewidget.cpp
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winnativewidget.h: note: No relevant classes found. No output generated.
[109/515] Building C object CMakeFiles/decoder_spi_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[110/515] Building C object CMakeFiles/decoder_uart_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[111/515] Building C object CMakeFiles/decoder_i2c_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[112/515] Building C object CMakeFiles/decoder_i2c_c.dir/libsigrokdecode/c_decoders/i2c_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c: In function 'i2c_handle_packet.part.0':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:215:47: warning: '%s' directive output may be truncated writing up to 255 bytes into a region of size between 251 and 506 [-Wformat-truncation=]
  215 |         snprintf(full, sizeof(full), "%s [SR] %s", s->packet_str, pkt_str);
      |                                               ^~
......
  238 |     i2c_format_packet(di, s, pkt_str, sizeof(pkt_str), pkt_short, sizeof(pkt_short));
      |                              ~~~~~~~
In function 'i2c_format_packet',
    inlined from 'i2c_handle_packet.part.0' at C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:238:5:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:215:9: note: 'snprintf' output between 7 and 517 bytes into a destination of size 512
  215 |         snprintf(full, sizeof(full), "%s [SR] %s", s->packet_str, pkt_str);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c: In function 'i2c_handle_packet.part.0':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:216:59: warning: '%s' directive output may be truncated writing up to 255 bytes into a region of size between 251 and 506 [-Wformat-truncation=]
  216 |         snprintf(full_short, sizeof(full_short), "%s [SR] %s", s->packet_str_short, pkt_short);
      |                                                           ^~
......
  238 |     i2c_format_packet(di, s, pkt_str, sizeof(pkt_str), pkt_short, sizeof(pkt_short));
      |                                                        ~~~~~~~~~
In function 'i2c_format_packet',
    inlined from 'i2c_handle_packet.part.0' at C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:238:5:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:216:9: note: 'snprintf' output between 7 and 517 bytes into a destination of size 512
  216 |         snprintf(full_short, sizeof(full_short), "%s [SR] %s", s->packet_str_short, pkt_short);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:217:9: warning: 'strncpy' output may be truncated copying 255 bytes from a string of length 511 [-Wstringop-truncation]
  217 |         strncpy(pkt_str, full, pkt_str_size - 1);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:218:9: warning: 'strncpy' output may be truncated copying 255 bytes from a string of length 511 [-Wstringop-truncation]
  218 |         strncpy(pkt_short, full_short, pkt_short_size - 1);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c: In function 'i2c_handle_packet.part.0':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:242:9: warning: 'strncpy' output may be truncated copying 255 bytes from a string of length 255 [-Wstringop-truncation]
  242 |         strncpy(s->packet_str, pkt_str, sizeof(s->packet_str) - 1);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/i2c_c.c:243:9: warning: 'strncpy' output may be truncated copying 255 bytes from a string of length 255 [-Wstringop-truncation]
  243 |         strncpy(s->packet_str_short, pkt_short, sizeof(s->packet_str_short) - 1);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[117/515] Building C object CMakeFiles/decoder_can_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[118/515] Building C object CMakeFiles/decoder_swd_c.dir/libsigrokdecode/c_decoders/swd_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/swd_c.c: In function 'swd_decode':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/swd_c.c:268:21: warning: unused variable 'parity_bit' [-Wunused-variable]
  268 |                 int parity_bit = s->bits[s->bits_len - 3] - '0';
      |                     ^~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/swd_c.c:366:51: warning: '%d' directive output may be truncated writing between 1 and 4 bytes into a region of size between 0 and 3 [-Wformat-truncation=]
  366 |                 snprintf(ptext, sizeof(ptext), "%d%d", s->dparity, parity_received);
      |                                                   ^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/swd_c.c:366:48: note: directive argument in the range [-176, 79]
  366 |                 snprintf(ptext, sizeof(ptext), "%d%d", s->dparity, parity_received);
      |                                                ^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/swd_c.c:366:17: note: 'snprintf' output between 3 and 16 bytes into a destination of size 4
  366 |                 snprintf(ptext, sizeof(ptext), "%d%d", s->dparity, parity_received);
      |                 ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[119/515] Building C object CMakeFiles/decoder_jtag_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[120/515] Building C object CMakeFiles/decoder_swd_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[123/515] Building C object CMakeFiles/decoder_onewire_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[124/515] Building C object CMakeFiles/decoder_lin_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[125/515] Building C object CMakeFiles/decoder_i2s_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[128/515] Building C object CMakeFiles/decoder_hdlc_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[131/515] Building C object CMakeFiles/decoder_microwire_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[132/515] Building C object CMakeFiles/decoder_ps2_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[133/515] Building C object CMakeFiles/decoder_mdio_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[134/515] Building C object CMakeFiles/decoder_mdio_c.dir/libsigrokdecode/c_decoders/mdio_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/mdio_c.c: In function 'mdio_state_ST':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/mdio_c.c:335:52: warning: unused parameter 'di' [-Wunused-parameter]
  335 | static void mdio_state_ST(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
      |                           ~~~~~~~~~~~~~~~~~~~~~~~~~^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/mdio_c.c:335:96: warning: unused parameter 'samplenum' [-Wunused-parameter]
  335 | static void mdio_state_ST(struct srd_decoder_inst *di, struct mdio_priv *s, int mdio, uint64_t samplenum)
      |                                                                                       ~~~~~~~~~^~~~~~~~~
[135/515] Building C object CMakeFiles/decoder_dmx512_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[136/515] Building C object CMakeFiles/decoder_dmx512_c.dir/libsigrokdecode/c_decoders/dmx512_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/dmx512_c.c: In function 'dmx_decode':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/dmx512_c.c:222:26: warning: variable 'bit_start' set but not used [-Wunused-but-set-variable]
  222 |                 uint64_t bit_start;
      |                          ^~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/dmx512_c.c:217:17: warning: variable 'bit_pos' set but not used [-Wunused-but-set-variable]
  217 |             int bit_pos[11];
      |                 ^~~~~~~
[138/515] Building C object CMakeFiles/decoder_nrzi_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[140/515] Building C object CMakeFiles/decoder_ir_nec_c.dir/libsigrokdecode/c_decoders/ir_nec_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c: In function 'putd':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:216:53: warning: '%0*X' directive output may be truncated writing between 2 and 536870911 bytes into a region of size 60 [-Wformat-truncation=]
  216 |         snprintf(long_str, sizeof(long_str), "%s: 0x%0*X", name, hex_width, data_val);
      |                                                     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:216:46: note: directive argument in the range [0, 65535]
  216 |         snprintf(long_str, sizeof(long_str), "%s: 0x%0*X", name, hex_width, data_val);
      |                                              ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:216:9: note: 'snprintf' output 7 or more bytes (assuming 536870916) into a destination of size 64
  216 |         snprintf(long_str, sizeof(long_str), "%s: 0x%0*X", name, hex_width, data_val);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:217:51: warning: '%0*X' directive output may be truncated writing between 2 and 536870911 bytes into a region of size 28 [-Wformat-truncation=]
  217 |         snprintf(mid_str, sizeof(mid_str), "%s: 0x%0*X", short_name, hex_width, data_val);
      |                                                   ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:217:44: note: directive argument in the range [0, 65535]
  217 |         snprintf(mid_str, sizeof(mid_str), "%s: 0x%0*X", short_name, hex_width, data_val);
      |                                            ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:217:9: note: 'snprintf' output 7 or more bytes (assuming 536870916) into a destination of size 32
  217 |         snprintf(mid_str, sizeof(mid_str), "%s: 0x%0*X", short_name, hex_width, data_val);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:218:53: warning: '%0*X' directive output may be truncated writing between 2 and 536870911 bytes into a region of size 12 [-Wformat-truncation=]
  218 |         snprintf(mid2_str, sizeof(mid2_str), "%s: 0x%0*X", shortest, hex_width, data_val);
      |                                                     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:218:46: note: directive argument in the range [0, 65535]
  218 |         snprintf(mid2_str, sizeof(mid2_str), "%s: 0x%0*X", shortest, hex_width, data_val);
      |                                              ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:218:9: note: 'snprintf' output 7 or more bytes (assuming 536870916) into a destination of size 16
  218 |         snprintf(mid2_str, sizeof(mid2_str), "%s: 0x%0*X", shortest, hex_width, data_val);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:211:53: warning: '%0*X' directive output may be truncated writing between 1 and 536870912 bytes into a region of size 60 [-Wformat-truncation=]
  211 |         snprintf(long_str, sizeof(long_str), "%s: 0x%0*X", name, hex_width, (uint8_t)data_val);
      |                                                     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:211:46: note: directive argument in the range [0, 255]
  211 |         snprintf(long_str, sizeof(long_str), "%s: 0x%0*X", name, hex_width, (uint8_t)data_val);
      |                                              ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:211:9: note: 'snprintf' output 6 or more bytes (assuming 536870917) into a destination of size 64
  211 |         snprintf(long_str, sizeof(long_str), "%s: 0x%0*X", name, hex_width, (uint8_t)data_val);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:212:51: warning: '%0*X' directive output may be truncated writing between 1 and 536870912 bytes into a region of size 28 [-Wformat-truncation=]
  212 |         snprintf(mid_str, sizeof(mid_str), "%s: 0x%0*X", short_name, hex_width, (uint8_t)data_val);
      |                                                   ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:212:44: note: directive argument in the range [0, 255]
  212 |         snprintf(mid_str, sizeof(mid_str), "%s: 0x%0*X", short_name, hex_width, (uint8_t)data_val);
      |                                            ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:212:9: note: 'snprintf' output 6 or more bytes (assuming 536870917) into a destination of size 32
  212 |         snprintf(mid_str, sizeof(mid_str), "%s: 0x%0*X", short_name, hex_width, (uint8_t)data_val);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:213:53: warning: '%0*X' directive output may be truncated writing between 1 and 536870912 bytes into a region of size 12 [-Wformat-truncation=]
  213 |         snprintf(mid2_str, sizeof(mid2_str), "%s: 0x%0*X", shortest, hex_width, (uint8_t)data_val);
      |                                                     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:213:46: note: directive argument in the range [0, 255]
  213 |         snprintf(mid2_str, sizeof(mid2_str), "%s: 0x%0*X", shortest, hex_width, (uint8_t)data_val);
      |                                              ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ir_nec_c.c:213:9: note: 'snprintf' output 6 or more bytes (assuming 536870917) into a destination of size 16
  213 |         snprintf(mid2_str, sizeof(mid2_str), "%s: 0x%0*X", shortest, hex_width, (uint8_t)data_val);
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[141/515] Building C object CMakeFiles/decoder_dcf77_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[142/515] Building C object CMakeFiles/decoder_ir_nec_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[143/515] Building C object CMakeFiles/decoder_ir_rc5_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[146/515] Building C object CMakeFiles/decoder_cec_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[147/515] Building C object CMakeFiles/decoder_spdif_c.dir/libsigrokdecode/c_decoders/spdif_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/spdif_c.c:102:12: warning: 'get_pulse_type_for_width' defined but not used [-Wunused-function]
  102 | static int get_pulse_type_for_width(struct spdif_priv *s, uint64_t width)
      |            ^~~~~~~~~~~~~~~~~~~~~~~~
[148/515] Building C object CMakeFiles/decoder_spdif_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[152/515] Building C object CMakeFiles/decoder_usb_signalling_c.dir/libsigrokdecode/c_decoders/usb_signalling_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/usb_signalling_c.c:497:1: warning: missing initializer for field 'recv_proto' of 'struct srd_c_decoder' [-Wmissing-field-initializers]
  497 | };
      | ^
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/usb_signalling_c.c:5:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode.h:434:12: note: 'recv_proto' declared here
  434 |     void (*recv_proto)(struct srd_decoder_inst *di,
      |            ^~~~~~~~~~
[153/515] Building C object CMakeFiles/decoder_usb_signalling_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[154/515] Building C object CMakeFiles/decoder_4b5b_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[155/515] Building C object CMakeFiles/decoder_can_fd_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[156/515] Building C object CMakeFiles/decoder_iso7816_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[157/515] Building C object CMakeFiles/decoder_lpc_c.dir/libsigrokdecode/c_decoders/lpc_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/lpc_c.c:101:20: warning: 'lpc_size_names' defined but not used [-Wunused-variable]
  101 | static const char *lpc_size_names[] = {
      |                    ^~~~~~~~~~~~~~
[158/515] Building C object CMakeFiles/decoder_lpc_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[159/515] Building C object CMakeFiles/decoder_dali_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[161/515] Building C object CMakeFiles/decoder_iso7816_c.dir/libsigrokdecode/c_decoders/iso7816_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/iso7816_c.c: In function 'handle_pps':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/iso7816_c.c:590:13: warning: unused variable 'byte_val' [-Wunused-variable]
  590 |     uint8_t byte_val;
      |             ^~~~~~~~
[162/515] Building C object CMakeFiles/decoder_can_fd_c.dir/libsigrokdecode/c_decoders/can_fd_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/can_fd_c.c: In function 'putg':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/can_fd_c.c:127:82: warning: unused parameter 'num_txts' [-Wunused-parameter]
  127 |                  uint64_t ss, uint64_t es, int ann_class, const char **txts, int num_txts)
      |                                                                              ~~~~^~~~~~~~
[164/515] Building C object CMakeFiles/decoder_c2_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[166/515] Building C object CMakeFiles/decoder_graycode_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[168/515] Building C object CMakeFiles/decoder_counter_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[169/515] Building C object CMakeFiles/decoder_lm75_c.dir/libsigrokdecode/c_decoders/lm75_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/lm75_c.c: In function 'lm75_handle_reg_0x01':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/lm75_c.c:125:101: warning: unused parameter 'rw' [-Wunused-parameter]
  125 | static void lm75_handle_reg_0x01(struct srd_decoder_inst *di, lm75_state *s, uint8_t b, const char *rw)
      |                                                                                         ~~~~~~~~~~~~^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/lm75_c.c: In function 'lm75_decode':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/lm75_c.c:241:50: warning: unused parameter 'di' [-Wunused-parameter]
  241 | static void lm75_decode(struct srd_decoder_inst *di)
      |                         ~~~~~~~~~~~~~~~~~~~~~~~~~^~
[170/515] Building C object CMakeFiles/decoder_lm75_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[171/515] Building C object CMakeFiles/decoder_ds1307_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[172/515] Building C object CMakeFiles/decoder_numbers_and..._c.dir/libsigrokdecode/c_decoders/numbers_and_state_c.c.ob
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/numbers_and_state_c.c: In function 'nas_format_value':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/numbers_and_state_c.c:330:66: warning: unused parameter 'pattern' [-Wunused-parameter]
  330 | static int nas_format_value(nas_state *s, double value, uint64_t pattern, char *buf, int bufsize)
      |                                                         ~~~~~~~~~^~~~~~~
[173/515] Building C object CMakeFiles/decoder_numbers_and_state_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[174/515] Building C object CMakeFiles/decoder_ds3231_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[175/515] Building C object CMakeFiles/decoder_ds1307_c.dir/libsigrokdecode/c_decoders/ds1307_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds1307_c.c: In function 'ds1307_decode':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds1307_c.c:366:52: warning: unused parameter 'di' [-Wunused-parameter]
  366 | static void ds1307_decode(struct srd_decoder_inst *di)
      |                           ~~~~~~~~~~~~~~~~~~~~~~~~~^~
[178/515] Building C object CMakeFiles/decoder_seven_segment_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[180/515] Building C object CMakeFiles/decoder_pwm_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[181/515] Building C object CMakeFiles/decoder_ds3231_c.dir/libsigrokdecode/c_decoders/ds3231_c.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds3231_c.c: In function 'ds3231_decode':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds3231_c.c:595:52: warning: unused parameter 'di' [-Wunused-parameter]
  595 | static void ds3231_decode(struct srd_decoder_inst *di)
      |                           ~~~~~~~~~~~~~~~~~~~~~~~~~^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds3231_c.c: In function 'ds3231_output_datetime':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds3231_c.c:147:45: warning: '%s' directive output may be truncated writing up to 127 bytes into a region of size 116 [-Wformat-truncation=]
  147 |     snprintf(t2, sizeof(t2), "%s date/time: %s", rw, t);
      |                                             ^~       ~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoders/ds3231_c.c:147:5: note: 'snprintf' output 13 or more bytes (assuming 140) into a destination of size 128
  147 |     snprintf(t2, sizeof(t2), "%s date/time: %s", rw, t);
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[184/515] Building C object CMakeFiles/decoder_ir_sirc_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[185/515] Building C object CMakeFiles/decoder_wiegand_c.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[226/515] Building C object CMakeFiles/PXView.dir/common/minizip/zip.c.obj
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/zip.c:186:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/crypt.h: In function 'decrypt_byte':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/crypt.h:35:62: warning: unused parameter 'pcrc_32_tab' [-Wunused-parameter]
   35 | static int decrypt_byte(unsigned long* pkeys, const z_crc_t* pcrc_32_tab)
      |                                               ~~~~~~~~~~~~~~~^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/zip.c: In function 'zip64local_SearchCentralDir':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/zip.c:521:5: warning: this 'for' clause does not guard... [-Wmisleading-indentation]
  521 |     for (i=(int)uReadSize-3; (i--)>0;)
      |     ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/zip.c:529:7: note: ...this statement, but the latter is misleadingly indented as if it were guarded by the 'for'
  529 |       if (uPosFound!=0)
      |       ^~
[230/515] Building C object CMakeFiles/PXView.dir/common/minizip/ioapi.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fopen_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:99:49: warning: unused parameter 'opaque' [-Wunused-parameter]
   99 | static voidpf ZCALLBACK fopen_file_func (voidpf opaque, const char* filename, int mode)
      |                                          ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fopen64_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:117:51: warning: unused parameter 'opaque' [-Wunused-parameter]
  117 | static voidpf ZCALLBACK fopen64_file_func (voidpf opaque, const void* filename, int mode)
      |                                            ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fread_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:165:48: warning: unused parameter 'opaque' [-Wunused-parameter]
  165 | static uLong ZCALLBACK fread_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
      |                                         ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fwrite_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:172:49: warning: unused parameter 'opaque' [-Wunused-parameter]
  172 | static uLong ZCALLBACK fwrite_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size)
      |                                          ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'ftell_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:179:47: warning: unused parameter 'opaque' [-Wunused-parameter]
  179 | static long ZCALLBACK ftell_file_func (voidpf opaque, voidpf stream)
      |                                        ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'ftell64_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:187:53: warning: unused parameter 'opaque' [-Wunused-parameter]
  187 | static ZPOS64_T ZCALLBACK ftell64_file_func (voidpf opaque, voidpf stream)
      |                                              ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fseek_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:194:48: warning: unused parameter 'opaque' [-Wunused-parameter]
  194 | static long ZCALLBACK fseek_file_func (voidpf  opaque, voidpf stream, uLong offset, int origin)
      |                                        ~~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fseek64_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:217:50: warning: unused parameter 'opaque' [-Wunused-parameter]
  217 | static long ZCALLBACK fseek64_file_func (voidpf  opaque, voidpf stream, ZPOS64_T offset, int origin)
      |                                          ~~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'fclose_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:243:47: warning: unused parameter 'opaque' [-Wunused-parameter]
  243 | static int ZCALLBACK fclose_file_func (voidpf opaque, voidpf stream)
      |                                        ~~~~~~~^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c: In function 'ferror_file_func':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/common/minizip/ioapi.c:250:47: warning: unused parameter 'opaque' [-Wunused-parameter]
  250 | static int ZCALLBACK ferror_file_func (voidpf opaque, voidpf stream)
      |                                        ~~~~~~~^~~~~~
[239/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/prop/bool.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/bool.cpp: In member function 'virtual QWidget* pv::prop::Bool::get_widget(QWidget*, bool)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/bool.cpp:62:41: warning: 'void QCheckBox::stateChanged(int)' is deprecated: Use checkStateChanged() instead [-Wdeprecated-declarations]
   62 |         connect(_check_box, &QCheckBox::stateChanged,
      |                                         ^~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtWidgets/QCheckBox:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/bool.cpp:26:
D:/msys64/mingw64/include/qt6/QtWidgets/qcheckbox.h:42:10: note: declared here
   42 |     void stateChanged(int);
      |          ^~~~~~~~~~~~
[245/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/data/sessiondocument.cpp.obj
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessiondocument.cpp:12:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessiondocument.h: In constructor 'pv::data::SessionDocument::SessionDocument()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessiondocument.h:148:22: warning: 'pv::data::SessionDocument::_decoder_model' will be initialized after [-Wreorder]
  148 |     DecoderModel    *_decoder_model;
      |                      ^~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessiondocument.h:118:14: warning:   'uint64_t pv::data::SessionDocument::_dock_sample_rate' [-Wreorder]
  118 |     uint64_t _dock_sample_rate;
      |              ^~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessiondocument.cpp:25:1: warning:   when initialized here [-Wreorder]
   25 | SessionDocument::SessionDocument() :
      | ^~~~~~~~~~~~~~~
[246/515] Building CXX object CMakeFiles/PXView.dir/PXView/main.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/main.cpp: In function 'int main(int, char**)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/main.cpp:91:9: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
   91 |         (void*)(argc);
      |         ^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/main.cpp:91:9: warning: statement has no effect [-Wunused-value]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/main.cpp:92:9: warning: statement has no effect [-Wunused-value]
   92 |         (void*)(argv);
      |         ^~~~~~~~~~~~~
[247/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dialogs/deviceoptions.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/deviceoptions.cpp: In constructor 'ChannelLabel::ChannelLabel(IChannelCheck*, QWidget*, int)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/deviceoptions.cpp:76:31: warning: 'void QCheckBox::stateChanged(int)' is deprecated: Use checkStateChanged() instead [-Wdeprecated-declarations]
   76 |     connect(_box, &QCheckBox::stateChanged, this, [this](){ update(); });
      |                               ^~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtWidgets/QCheckBox:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/deviceoptions.h:37,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/deviceoptions.cpp:24:
D:/msys64/mingw64/include/qt6/QtWidgets/qcheckbox.h:42:10: note: declared here
   42 |     void stateChanged(int);
      |          ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/deviceoptions.cpp: In member function 'void pv::dialogs::DeviceOptions::analog_probes(QGridLayout&)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/deviceoptions.cpp:763:20: warning: comparison of integer expressions of different signedness: 'int' and 'std::vector<bool>::size_type' {aka 'long long unsigned int'} [-Wsign-compare]
  763 |         if (ch_dex < _lst_probe_enabled_status.size()){
      |             ~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[253/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/view/ruler.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/ruler.cpp: In member function 'void pv::view::Ruler::draw_cursor_sel(QPainter&)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/ruler.cpp:741:14: warning: variable 'i' set but not used [-Wunused-but-set-variable]
  741 |         auto i = cursor_list.begin();
      |              ^
[254/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/view/cursor.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/cursor.cpp: In constructor 'pv::view::Cursor::Cursor(pv::view::View&, int, uint64_t)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/cursor.cpp:50:32: warning: unused parameter 'order' [-Wunused-parameter]
   50 | Cursor::Cursor(View &view, int order, uint64_t sampleIndex) :
      |                            ~~~~^~~~~
[255/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/mainwindow.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp: In member function 'int pv::MainWindow::resolveShortcutAction(int, int)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp:1767:33: warning: 'constexpr QKeyCombination::operator int() const' is deprecated: Use QKeyCombination instead of int [-Wdeprecated-declarations]
 1767 |             int combined = seq[0];
      |                            ~~~~~^
In file included from D:/msys64/mingw64/include/qt6/QtCore/qobjectdefs.h:12,
                 from D:/msys64/mingw64/include/qt6/QtGui/qwindowdefs.h:8,
                 from D:/msys64/mingw64/include/qt6/QtWidgets/qwidget.h:9,
                 from D:/msys64/mingw64/include/qt6/QtWidgets/QWidget:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/widgets/searchpatterninput.h:26,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp:24:
D:/msys64/mingw64/include/qt6/QtCore/qnamespace.h:1946:26: note: declared here
 1946 |     constexpr Q_IMPLICIT operator int() const noexcept
      |                          ^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp: In member function 'virtual void pv::MainWindow::switchLanguage(int)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp:2068:18: warning: ignoring return value of 'bool QTranslator::load(const QString&, const QString&, const QString&, const QString&)', declared with attribute 'nodiscard' [-Wunused-result]
 2068 |     _qtTrans.load(":/qt_" + QString::number(language));
      |     ~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtCore/QTranslator:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.h:31,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp:61:
D:/msys64/mingw64/include/qt6/QtCore/qtranslator.h:34:24: note: declared here
   34 |     [[nodiscard]] bool load(const QString & filename,
      |                        ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainwindow.cpp:2070:18: warning: ignoring return value of 'bool QTranslator::load(const QString&, const QString&, const QString&, const QString&)', declared with attribute 'nodiscard' [-Wunused-result]
 2070 |     _myTrans.load(":/my_" + QString::number(language));
      |     ~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
D:/msys64/mingw64/include/qt6/QtCore/qtranslator.h:34:24: note: declared here
   34 |     [[nodiscard]] bool load(const QString & filename,
      |                        ^~~~
[257/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/view/viewport.cpp.obj
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:24:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.h: In constructor 'pv::view::Viewport::Viewport(pv::view::View&, View_type)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.h:251:8: warning: 'pv::view::Viewport::_xcurs_moved' will be initialized after [-Wreorder]
  251 |   bool _xcurs_moved;
      |        ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.h:190:7: warning:   'int pv::view::Viewport::_curVOffset' [-Wreorder]
  190 |   int _curVOffset;
      |       ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:210:1: warning:   when initialized here [-Wreorder]
  210 | Viewport::Viewport(View &parent, View_type type)
      | ^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.h:275:8: warning: 'pv::view::Viewport::g_drag_active' will be initialized after [-Wreorder]
  275 |   bool g_drag_active;
      |        ^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.h:264:7: warning:   'int pv::view::Viewport::_paint_in_this_second' [-Wreorder]
  264 |   int _paint_in_this_second;
      |       ^~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:210:1: warning:   when initialized here [-Wreorder]
  210 | Viewport::Viewport(View &parent, View_type type)
      | ^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp: In member function 'virtual void pv::view::Viewport::mousePressEvent(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1040:15: warning: unused variable 'cursor_list' [-Wunused-variable]
 1040 |         auto &cursor_list = _view.get_cursorList();
      |               ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp: In member function 'virtual void pv::view::Viewport::mouseMoveEvent(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1169:7: warning: unused variable 'mode' [-Wunused-variable]
 1169 |   int mode = _view.session().get_device()->get_work_mode();
      |       ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp: In member function 'void pv::view::Viewport::onLogicMouseRelease(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'CURS_MOVE' not handled in switch [-Wswitch]
 1248 |   switch (_action_type) {
      |          ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'RESIZE_SIGNAL' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'DSO_XM_STEP0' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'DSO_XM_STEP1' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'DSO_XM_STEP2' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'DSO_YM' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1248:10: warning: enumeration value 'DSO_TRIG_MOVE' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp: In member function 'void pv::view::Viewport::onDsoMouseRelease(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1388:10: warning: enumeration value 'CURS_MOVE' not handled in switch [-Wswitch]
 1388 |   switch (_action_type) {
      |          ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1388:10: warning: enumeration value 'LOGIC_EDGE' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1388:10: warning: enumeration value 'LOGIC_MOVE' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1388:10: warning: enumeration value 'LOGIC_ZOOM' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1388:10: warning: enumeration value 'LOGIC_JUMP' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1388:10: warning: enumeration value 'RESIZE_SIGNAL' not handled in switch [-Wswitch]
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp: In member function 'void pv::view::Viewport::onAnalogMouseRelease(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1467:50: warning: unused parameter 'event' [-Wunused-parameter]
 1467 | void Viewport::onAnalogMouseRelease(QMouseEvent *event) {}
      |                                     ~~~~~~~~~~~~~^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp: In member function 'virtual void pv::view::Viewport::mouseDoubleClickEvent(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1607:13: warning: unused variable 'cursor_list' [-Wunused-variable]
 1607 |       auto &cursor_list = _view.get_cursorList();
      |             ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/viewport.cpp:1634:13: warning: unused variable 'cursor_list' [-Wunused-variable]
 1634 |       auto &cursor_list = _view.get_cursorList();
      |             ^~~~~~~~~~~
[258/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/view/view.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp: In member function 'void pv::view::View::signals_changed(const pv::view::Trace*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp:982:11: warning: unused variable 'max_height' [-Wunused-variable]
  982 |   uint8_t max_height = MaxHeightUnit;
      |           ^~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp: In member function 'virtual void pv::view::View::resizeEvent(QResizeEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp:1264:38: warning: unused parameter 'event' [-Wunused-parameter]
 1264 | void View::resizeEvent(QResizeEvent *event) {
      |                        ~~~~~~~~~~~~~~^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp: In member function 'void pv::view::View::add_cursor(QColor, uint64_t)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp:1415:30: warning: unused parameter 'color' [-Wunused-parameter]
 1415 | void View::add_cursor(QColor color, uint64_t sampleIndex) {
      |                       ~~~~~~~^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp: In member function 'void pv::view::View::rebuild_signals()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp:1930:32: warning: comparison of integer expressions of different signedness: 'std::vector<pv::data::ChannelConfig>::size_type' {aka 'long long unsigned int'} and 'int' [-Wsign-compare]
 1930 |     if (config.channels.size() == device_ch_count) {
      |         ~~~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~
[260/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/view/dsldial.cpp.obj
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/log.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/dsldial.cpp:27:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/dsldial.cpp: In member function 'void pv::view::dslDial::paint(QPainter&, QRectF, QColor, QPoint, QString&)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/dsldial.cpp:103:25: warning: comparison of integer expressions of different signedness: 'uint64_t' {aka 'long long unsigned int'} and 'qsizetype' {aka 'long long int'} [-Wsign-compare]
  103 |     assert(displayIndex < _unit.count());
      |            ~~~~~~~~~~~~~^~~~~~~~~~~~~~~
[263/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/prop/binding/deviceoptions.cpp.obj
In file included from D:/msys64/mingw64/include/boost/none_t.hpp:17,
                 from D:/msys64/mingw64/include/boost/none.hpp:17,
                 from D:/msys64/mingw64/include/boost/optional/optional.hpp:48,
                 from D:/msys64/mingw64/include/boost/optional.hpp:15,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/binding/deviceoptions.h:29,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/binding/deviceoptions.cpp:24:
D:/msys64/mingw64/include/boost/bind.hpp:36:1: note: '#pragma message: The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated. Please use <boost/bind/bind.hpp> + using namespace boost::placeholders, or define BOOST_BIND_GLOBAL_PLACEHOLDERS to retain the current behavior.'
   36 | BOOST_PRAGMA_MESSAGE(
      | ^~~~~~~~~~~~~~~~~~~~
[265/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/toolbars/filebar.cpp.obj
In file included from D:/msys64/mingw64/include/boost/bind.hpp:30,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/filebar.cpp:23:
D:/msys64/mingw64/include/boost/bind.hpp:36:1: note: '#pragma message: The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated. Please use <boost/bind/bind.hpp> + using namespace boost::placeholders, or define BOOST_BIND_GLOBAL_PLACEHOLDERS to retain the current behavior.'
   36 | BOOST_PRAGMA_MESSAGE(
      | ^~~~~~~~~~~~~~~~~~~~
[267/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dock/measuredock.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp: In constructor 'pv::dock::MeasureDock::MeasureDock(QWidget*, pv::view::View*, pv::SigSession*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp:211:40: warning: 'void QCheckBox::stateChanged(int)' is deprecated: Use checkStateChanged() instead [-Wdeprecated-declarations]
  211 |     connect(_fen_checkBox, &QCheckBox::stateChanged, _view, &view::View::set_measure_en);
      |                                        ^~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtWidgets/QCheckBox:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.h:34,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp:26:
D:/msys64/mingw64/include/qt6/QtWidgets/qcheckbox.h:42:10: note: declared here
   42 |     void stateChanged(int);
      |          ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp: In member function 'void pv::dock::MeasureDock::set_view(pv::view::View*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp:232:47: warning: 'void QCheckBox::stateChanged(int)' is deprecated: Use checkStateChanged() instead [-Wdeprecated-declarations]
  232 |         disconnect(_fen_checkBox, &QCheckBox::stateChanged, _view, &view::View::set_measure_en);
      |                                               ^~~~~~~~~~~~
D:/msys64/mingw64/include/qt6/QtWidgets/qcheckbox.h:42:10: note: declared here
   42 |     void stateChanged(int);
      |          ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp:242:44: warning: 'void QCheckBox::stateChanged(int)' is deprecated: Use checkStateChanged() instead [-Wdeprecated-declarations]
  242 |         connect(_fen_checkBox, &QCheckBox::stateChanged, _view, &view::View::set_measure_en);
      |                                            ^~~~~~~~~~~~
D:/msys64/mingw64/include/qt6/QtWidgets/qcheckbox.h:42:10: note: declared here
   42 |     void stateChanged(int);
      |          ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp: In member function 'void pv::dock::MeasureDock::update_dist()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/measuredock.cpp:819:12: warning: variable 'bkColor' set but not used [-Wunused-but-set-variable]
  819 |     QColor bkColor = AppConfig::Instance().GetStyleColor();
      |            ^~~~~~~
[271/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/toolbars/logobar.cpp.obj
In file included from D:/msys64/mingw64/include/boost/bind.hpp:30,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/logobar.cpp:23:
D:/msys64/mingw64/include/boost/bind.hpp:36:1: note: '#pragma message: The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated. Please use <boost/bind/bind.hpp> + using namespace boost::placeholders, or define BOOST_BIND_GLOBAL_PLACEHOLDERS to retain the current behavior.'
   36 | BOOST_PRAGMA_MESSAGE(
      | ^~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/logobar.cpp: In member function 'void pv::toolbars::LogoBar::enable_toggle(bool)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/logobar.cpp:206:34: warning: unused parameter 'enable' [-Wunused-parameter]
  206 | void LogoBar::enable_toggle(bool enable)
      |                             ~~~~~^~~~~~
[276/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/prop/binding/decoderoptions.cpp.obj
In file included from D:/msys64/mingw64/include/boost/bind.hpp:30,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/binding/decoderoptions.cpp:25:
D:/msys64/mingw64/include/boost/bind.hpp:36:1: note: '#pragma message: The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated. Please use <boost/bind/bind.hpp> + using namespace boost::placeholders, or define BOOST_BIND_GLOBAL_PLACEHOLDERS to retain the current behavior.'
   36 | BOOST_PRAGMA_MESSAGE(
      | ^~~~~~~~~~~~~~~~~~~~
[280/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dock/searchdock.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp: In member function 'bool pv::dock::SearchDock::gpu_edge_search_worker(pv::data::LogicSnapshot*, int64_t, const std::map<short unsigned int, QString>&, std::vector<pv::dock::SearchData>&, QElapsedTimer&, bool&, bool&)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp:537:62: warning: unused parameter 'logic_snapshot' [-Wunused-parameter]
  537 | bool SearchDock::gpu_edge_search_worker(data::LogicSnapshot *logic_snapshot,
      |                                         ~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp:538:49: warning: unused parameter 'end' [-Wunused-parameter]
  538 |                                         int64_t end,
      |                                         ~~~~~~~~^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp:540:66: warning: unused parameter 'local_batch' [-Wunused-parameter]
  540 |                                         std::vector<SearchData> &local_batch,
      |                                         ~~~~~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp:541:56: warning: unused parameter 'ui_timer' [-Wunused-parameter]
  541 |                                         QElapsedTimer &ui_timer,
      |                                         ~~~~~~~~~~~~~~~^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp:542:47: warning: unused parameter 'has_new_results' [-Wunused-parameter]
  542 |                                         bool &has_new_results,
      |                                         ~~~~~~^~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/searchdock.cpp:543:47: warning: unused parameter 'first_flush' [-Wunused-parameter]
  543 |                                         bool &first_flush) {
      |                                         ~~~~~~^~~~~~~~~~~
[286/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dock/deviceoptionsdock.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/deviceoptionsdock.cpp: In member function 'void pv::dock::DeviceOptionsDock::mode_check_timeout()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/deviceoptionsdock.cpp:522:22: warning: ignoring return value of 'auto QtConcurrent::run(Function&&, Args&& ...) [with Function = pv::dock::DeviceOptionsDock::mode_check_timeout()::<lambda()>; Args = {}]', declared with attribute 'nodiscard': 'Use QThreadPool::start(Callable&&) if you don't need the returned QFuture' [-Wunused-result]
  522 |     QtConcurrent::run([this, agent, saved_opt_mode]() {
      |     ~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  523 |       int mode;
      |       ~~~~~~~~~
  524 |       bool got_mode = agent->get_config_int16(SR_CONF_OPERATION_MODE, mode);
      |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  525 |       if (!got_mode || mode == saved_opt_mode)
      |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  526 |         return;
      |         ~~~~~~~
  527 |
      |
  528 |       QMetaObject::invokeMethod(this, [this, mode]() {
      |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  529 |         if (_isBuilding)
      |         ~~~~~~~~~~~~~~~~
  530 |           return;
      |           ~~~~~~~
  531 |         _opt_mode = mode;
      |         ~~~~~~~~~~~~~~~~~
  532 |         build_dynamic_panel();
      |         ~~~~~~~~~~~~~~~~~~~~~~
  533 |         try_resize_scroll();
      |         ~~~~~~~~~~~~~~~~~~~~
  534 |       });
      |       ~~~
  535 |     });
      |     ~~
In file included from D:/msys64/mingw64/include/qt6/QtConcurrent/QtConcurrent:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/deviceoptionsdock.cpp:39:
D:/msys64/mingw64/include/qt6/QtConcurrent/qtconcurrentrun.h:63:6: note: declared here
   63 | auto run(Function &&f, Args &&...args)
      |      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/deviceoptionsdock.cpp:537:22: warning: ignoring return value of 'auto QtConcurrent::run(Function&&, Args&& ...) [with Function = pv::dock::DeviceOptionsDock::mode_check_timeout()::<lambda()>; Args = {}]', declared with attribute 'nodiscard': 'Use QThreadPool::start(Callable&&) if you don't need the returned QFuture' [-Wunused-result]
  537 |     QtConcurrent::run([this, agent]() {
      |     ~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~
  538 |       bool test;
      |       ~~~~~~~~~~
  539 |       bool got_test = agent->get_config_bool(SR_CONF_TEST, test);
      |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  540 |       if (!got_test || !test)
      |       ~~~~~~~~~~~~~~~~~~~~~~~
  541 |         return;
      |         ~~~~~~~
  542 |
      |
  543 |       QMetaObject::invokeMethod(this, [this]() {
      |       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  544 |         setUpdatesEnabled(false);
      |         ~~~~~~~~~~~~~~~~~~~~~~~~~
  545 |         for (auto box : _probes_checkBox_list) {
      |         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  546 |           box->setCheckState(Qt::Checked);
      |           ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  547 |           box->setDisabled(true);
      |           ~~~~~~~~~~~~~~~~~~~~~~~
  548 |         }
      |         ~
  549 |         setUpdatesEnabled(true);
      |         ~~~~~~~~~~~~~~~~~~~~~~~~
  550 |       });
      |       ~~~
  551 |     });
      |     ~~
D:/msys64/mingw64/include/qt6/QtConcurrent/qtconcurrentrun.h:63:6: note: declared here
   63 | auto run(Function &&f, Args &&...args)
      |      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/deviceoptionsdock.cpp: In member function 'void pv::dock::DeviceOptionsDock::analog_probes(QGridLayout&)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/deviceoptionsdock.cpp:717:16: warning: comparison of integer expressions of different signedness: 'int' and 'std::vector<bool>::size_type' {aka 'long long unsigned int'} [-Wsign-compare]
  717 |     if (ch_dex < _lst_probe_enabled_status.size()) {
      |         ~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[293/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dialogs/storeprogress.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp: In member function 'virtual void pv::dialogs::StoreProgress::accept()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp:208:29: warning: comparison of integer expressions of different signedness: 'uint64_t' {aka 'long long unsigned int'} and 'int' [-Wsign-compare]
  208 |             if (start_index > total_count && end_index > total_count)
      |                 ~~~~~~~~~~~~^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp:208:56: warning: comparison of integer expressions of different signedness: 'uint64_t' {aka 'long long unsigned int'} and 'int' [-Wsign-compare]
  208 |             if (start_index > total_count && end_index > total_count)
      |                                              ~~~~~~~~~~^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp:180:15: warning: unused variable 'cursor_list' [-Wunused-variable]
  180 |         auto &cursor_list = _view->get_cursorList();
      |               ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp: In member function 'void pv::dialogs::StoreProgress::save_run(ISessionDataGetter*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp:280:24: warning: comparison of integer expressions of different signedness: 'int' and 'std::__cxx11::list<pv::view::Cursor*>::size_type' {aka 'long long unsigned int'} [-Wsign-compare]
  280 |         for (int i=0; i<cursor_list.size(); i++){
      |                       ~^~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp: In member function 'void pv::dialogs::StoreProgress::export_run()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/storeprogress.cpp:320:24: warning: comparison of integer expressions of different signedness: 'int' and 'std::__cxx11::list<pv::view::Cursor*>::size_type' {aka 'long long unsigned int'} [-Wsign-compare]
  320 |         for (int i=0; i<cursor_list.size(); i++){
      |                       ~^~~~~~~~~~~~~~~~~~~
[295/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/toolbars/titlebar.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/titlebar.cpp: In member function 'virtual void pv::toolbars::TitleBar::mouseMoveEvent(QMouseEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/titlebar.cpp:712:15: warning: variable 'rect' set but not used [-Wunused-but-set-variable]
  712 |         QRect rect = _parent->frameGeometry();
      |               ^~~~
[302/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/storesession.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/storesession.cpp: In member function 'bool pv::StoreSession::meta_gen(pv::data::Snapshot*, std::string&)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/storesession.cpp:546:22: warning: unused variable 'status' [-Wunused-variable]
  546 |     struct sr_status status;
      |                      ^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/storesession.cpp: In member function 'void pv::StoreSession::export_exec(pv::data::Snapshot*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/storesession.cpp:915:14: warning: ignoring return value of 'virtual bool QFile::open(QIODeviceBase::OpenMode)', declared with attribute 'nodiscard' [-Wunused-result]
  915 |     file.open(QIODevice::WriteOnly | QIODevice::Text);
      |     ~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtCore/qdir.h:11,
                 from D:/msys64/mingw64/include/qt6/QtWidgets/qfiledialog.h:9,
                 from D:/msys64/mingw64/include/qt6/QtWidgets/QFileDialog:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/storesession.cpp:42:
D:/msys64/mingw64/include/qt6/QtCore/qfile.h:264:32: note: declared here
  264 |     QFILE_MAYBE_NODISCARD bool open(OpenMode flags) override;
      |                                ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/storesession.cpp:1157:24: warning: unused variable 'read_buf' [-Wunused-variable]
 1157 |         unsigned char* read_buf = (unsigned char*)data_buffer;
      |                        ^~~~~~~~
[304/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dialogs/protocolexp.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/protocolexp.cpp: In member function 'void pv::dialogs::ProtocolExp::save_proc()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/protocolexp.cpp:202:14: warning: ignoring return value of 'virtual bool QFile::open(QIODeviceBase::OpenMode)', declared with attribute 'nodiscard' [-Wunused-result]
  202 |     file.open(QIODevice::WriteOnly | QIODevice::Text);
      |     ~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtCore/QFile:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dialogs/protocolexp.cpp:27:
D:/msys64/mingw64/include/qt6/QtCore/qfile.h:264:32: note: declared here
  264 |     QFILE_MAYBE_NODISCARD bool open(OpenMode flags) override;
      |                                ^~~~
[313/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/mainframe.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainframe.cpp: In member function 'virtual bool pv::MainFrame::eventFilter(QObject*, QEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainframe.cpp:415:9: warning: unused variable 'newWidth' [-Wunused-variable]
  415 |     int newWidth = 0;
      |         ^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainframe.cpp:416:9: warning: unused variable 'newHeight' [-Wunused-variable]
  416 |     int newHeight = 0;
      |         ^~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainframe.cpp:417:9: warning: unused variable 'newLeft' [-Wunused-variable]
  417 |     int newLeft = 0;
      |         ^~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/mainframe.cpp:418:9: warning: unused variable 'newTop' [-Wunused-variable]
  418 |     int newTop = 0;
      |         ^~~~~~
[318/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/submainframe.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/submainframe.cpp: In destructor 'virtual pv::SubMainFrame::~SubMainFrame()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/submainframe.cpp:144:9: warning: deleting object of polymorphic class type 'pv::WinNativeWidget' which has non-virtual destructor might cause undefined behavior [-Wdelete-non-virtual-dtor]
  144 |         delete _parentNativeWidget;
      |         ^~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/submainframe.cpp: In member function 'void pv::SubMainFrame::AttachNativeWindow()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/submainframe.cpp:183:9: warning: deleting object of polymorphic class type 'pv::WinNativeWidget' which has non-virtual destructor might cause undefined behavior [-Wdelete-non-virtual-dtor]
  183 |         delete nativeWindow;
      |         ^~~~~~~~~~~~~~~~~~~
[321/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/data/decode/annotationrestable.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/annotationrestable.cpp: In member function 'const char* AnnotationResTable::format_numberic(const char*, int)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/annotationrestable.cpp:296:34: warning: 'char* strncpy(char*, const char*, size_t)' output truncated before terminating nul copying as many bytes from a string as its length [-Wstringop-truncation]
  296 |                           strncpy(all_wr, sub_str, sublen);
      |                           ~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/annotationrestable.cpp:289:69: note: length computed here
  289 |                           unsigned int sublen = (unsigned int)strlen(sub_str);
      |                                                               ~~~~~~^~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/annotationrestable.cpp:324:25: warning: 'char* strncpy(char*, const char*, size_t)' output truncated before terminating nul copying as many bytes from a string as its length [-Wstringop-truncation]
  324 |                  strncpy(all_wr, sub_str, sublen);
      |                  ~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/decode/annotationrestable.cpp:316:60: note: length computed here
  316 |                  unsigned int sublen = (unsigned int)strlen(sub_str);
      |                                                      ~~~~~~^~~~~~~~~
[322/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/prop/binding/probeoptions.cpp.obj
In file included from D:/msys64/mingw64/include/boost/none_t.hpp:17,
                 from D:/msys64/mingw64/include/boost/none.hpp:17,
                 from D:/msys64/mingw64/include/boost/optional/optional.hpp:48,
                 from D:/msys64/mingw64/include/boost/optional.hpp:15,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/binding/probeoptions.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/prop/binding/probeoptions.cpp:23:
D:/msys64/mingw64/include/boost/bind.hpp:36:1: note: '#pragma message: The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated. Please use <boost/bind/bind.hpp> + using namespace boost::placeholders, or define BOOST_BIND_GLOBAL_PLACEHOLDERS to retain the current behavior.'
   36 | BOOST_PRAGMA_MESSAGE(
      | ^~~~~~~~~~~~~~~~~~~~
[340/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/ui/langresource.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/langresource.cpp: In member function 'void LangResource::load_page(Lang_resource_page&, QString)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/langresource.cpp:165:11: warning: ignoring return value of 'virtual bool QFile::open(QIODeviceBase::OpenMode)', declared with attribute 'nodiscard' [-Wunused-result]
  165 |     f.open(QFile::ReadOnly | QFile::Text);
      |     ~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/qt6/QtCore/QFile:1,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/langresource.cpp:27:
D:/msys64/mingw64/include/qt6/QtCore/qfile.h:264:32: note: declared here
  264 |     QFILE_MAYBE_NODISCARD bool open(OpenMode flags) override;
      |                                ^~~~
[349/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/dock/keywordlineedit.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/keywordlineedit.cpp: In member function 'void PopupLineEditInput::Popup(QWidget*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/dock/keywordlineedit.cpp:238:10: warning: variable 'pt' set but not used [-Wunused-but-set-variable]
  238 |   QPoint pt = mapToGlobal(editline->rect().bottomLeft());
      |          ^~
[355/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/ui/dscombobox.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/dscombobox.cpp: In member function 'void DsComboBox::measureSize()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/ui/dscombobox.cpp:152:9: warning: variable 'height' set but not used [-Wunused-but-set-variable]
  152 |     int height = 30;
      |         ^~~~~~
[370/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/winshadow.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winshadow.cpp: In member function 'void pv::WinShadow::moveShadow()':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winshadow.cpp:175:14: warning: unused variable 'isActiveWindow' [-Wunused-variable]
  175 |         bool isActiveWindow = ((active_window == m_hwnd)
      |              ^~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winshadow.cpp: In member function 'virtual void pv::WinShadow::paintEvent(QPaintEvent*)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winshadow.cpp:182:41: warning: unused parameter 'event' [-Wunused-parameter]
  182 | void WinShadow::paintEvent(QPaintEvent *event)
      |                            ~~~~~~~~~~~~~^~~~~
[375/515] Building CXX object CMakeFiles/PXView.dir/PXView/pv/winnativewidget.cpp.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winnativewidget.cpp: In member function 'LRESULT pv::WinNativeWidget::hitTest(HWND, WPARAM, LPARAM)':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/winnativewidget.cpp:392:52: warning: unused parameter 'wParam' [-Wunused-parameter]
  392 | LRESULT WinNativeWidget::hitTest(HWND hWnd, WPARAM wParam, LPARAM lParam)
      |                                             ~~~~~~~^~~~~~
[479/515] Building C object CMakeFiles/PXView.dir/libsigrok/hardware/demo/demo.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/demo/demo.c:75:6: warning: implicit conversion from 'enum DEMO_LOGIC_CHANNEL_ID' to 'enum DEMO_CHANNEL_ID' [-Wenum-conversion]
   75 |     {DEMO_LOGIC125x16,  LOGIC,  SR_CHANNEL_LOGIC,  16, 1, SR_MHZ(1), SR_Mn(1),
      |      ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/demo/demo.c:77:6: warning: implicit conversion from 'enum DEMO_LOGIC_CHANNEL_ID' to 'enum DEMO_CHANNEL_ID' [-Wenum-conversion]
   77 |     {DEMO_LOGIC250x12,  LOGIC,  SR_CHANNEL_LOGIC,  12, 1, SR_MHZ(1), SR_Mn(1),
      |      ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/demo/demo.c:79:6: warning: implicit conversion from 'enum DEMO_LOGIC_CHANNEL_ID' to 'enum DEMO_CHANNEL_ID' [-Wenum-conversion]
   79 |     {DEMO_LOGIC500x6,  LOGIC,  SR_CHANNEL_LOGIC,  6, 1, SR_MHZ(1), SR_Mn(1),
      |      ^~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/demo/demo.c:81:6: warning: implicit conversion from 'enum DEMO_LOGIC_CHANNEL_ID' to 'enum DEMO_CHANNEL_ID' [-Wenum-conversion]
   81 |     {DEMO_LOGIC1000x3,  LOGIC,  SR_CHANNEL_LOGIC,  3, 1, SR_MHZ(1), SR_Mn(1),
      |      ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/demo/demo.c: In function 'config_set':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/demo/demo.c:1138:41: warning: implicit conversion from 'enum DEMO_CHANNEL_ID' to 'enum DEMO_LOGIC_CHANNEL_ID' [-Wenum-conversion]
 1138 |                     vdev->logic_ch_mode = (enum DEMO_CHANNEL_ID)nv;
      |                                         ^
[485/515] Building C object CMakeFiles/PXView.dir/libsigrok/output/csv.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/output/csv.c: In function 'init':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/output/csv.c:72:9: warning: variable 'ch_num' set but not used [-Wunused-but-set-variable]
   72 |     int ch_num;
      |         ^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/output/csv.c: In function 'receive':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/output/csv.c:335:26: warning: comparison of integer expressions of different signedness: 'uint64_t' {aka 'long long unsigned int'} and 'int' [-Wsign-compare]
  335 |            for (j = 0; j < ch_num; j++) {
      |                          ^
[490/515] Building C object CMakeFiles/PXView.dir/libsigrok/hardware/DSL/dslogic.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dslogic.c: In function 'scan':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dslogic.c:292:5: warning: this 'else' clause does not guard... [-Wmisleading-indentation]
  292 |     else
      |     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dslogic.c:295:9: note: ...this statement, but the latter is misleadingly indented as if it were guarded by the 'else'
  295 |         conn = NULL;
      |         ^~~~
[491/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/type_decoder.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/type_decoder.c:22:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/type_decoder.c:21:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[492/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/srd.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/srd.c:32:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/srd.c:22:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/srd.c: In function 'srd_init':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/srd.c:301:5: warning: 'PyEval_InitThreads' is deprecated [-Wdeprecated-declarations]
  301 |     PyEval_InitThreads();
      |     ^~~~~~~~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/python3.14/Python.h:135:
D:/msys64/mingw64/include/python3.14/ceval.h:114:37: note: declared here
  114 | Py_DEPRECATED(3.9) PyAPI_FUNC(void) PyEval_InitThreads(void);
      |                                     ^~~~~~~~~~~~~~~~~~
[495/515] Building C object CMakeFiles/PXView.dir/libsigrok/hardware/DSL/dscope.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c: In function 'scan':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c:209:5: warning: this 'else' clause does not guard... [-Wmisleading-indentation]
  209 |     else
      |     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c:212:9: note: ...this statement, but the latter is misleadingly indented as if it were guarded by the 'else'
  212 |         conn = NULL;
      |         ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c: In function 'dso_offset':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c:457:32: warning: 'offset_coarse' may be used uninitialized [-Wmaybe-uninitialized]
  457 |                ((offset_coarse + DSCOPE_CONSTANT_BIAS + (preoff>>10)) << 16) + offset_fine +
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c:428:9: note: 'offset_coarse' was declared here
  428 |     int offset_coarse, offset_fine;
      |         ^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c:457:78: warning: 'offset_fine' may be used uninitialized [-Wmaybe-uninitialized]
  456 |         return (offset << 32) +
      |                ~~~~~~~~~~~~~~~~
  457 |                ((offset_coarse + DSCOPE_CONSTANT_BIAS + (preoff>>10)) << 16) + offset_fine +
      |                ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/DSL/dscope.c:428:24: note: 'offset_fine' was declared here
  428 |     int offset_coarse, offset_fine;
      |                        ^~~~~~~~~~~
[499/515] Building C object CMakeFiles/PXView.dir/libsigrok/hardware/pxlogic/usb_ctrl.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/usb_ctrl.c: In function 'usb_rd_data_req':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/usb_ctrl.c:304:127: warning: unused parameter 'buff' [-Wunused-parameter]
  304 | unsigned int usb_rd_data_req(libusb_device_handle *usbdevh,unsigned int base_addr,int length,unsigned int mode,unsigned char *buff,unsigned int timeout){
      |                                                                                                                ~~~~~~~~~~~~~~~^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/usb_ctrl.c:304:145: warning: unused parameter 'timeout' [-Wunused-parameter]
  304 | unsigned int usb_rd_data_req(libusb_device_handle *usbdevh,unsigned int base_addr,int length,unsigned int mode,unsigned char *buff,unsigned int timeout){
      |                                                                                                                                    ~~~~~~~~~~~~~^~~~~~~
[500/515] Building C object CMakeFiles/PXView.dir/libsigrok/lib_main.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/lib_main.c: In function 'post_event_proc':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/lib_main.c:1570:46: warning: cast from pointer to integer of different size [-Wpointer-to-int-cast]
 1570 |                 lib_ctx.event_callback((int)((unsigned long)event));
      |                                              ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/lib_main.c: In function 'post_event_async':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/lib_main.c:1586:58: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
 1586 |         g_thread_new("callback_thread", post_event_proc, (gpointer)((unsigned long)event));
      |                                                          ^
[501/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/module_sigrokdecode.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/module_sigrokdecode.c:21:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/module_sigrokdecode.c:20:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[502/515] Building C object CMakeFiles/PXView.dir/libsigrok/hardware/pxlogic/pxlogic.c.obj
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:23:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:5: warning: braces around scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |     ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:5: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:40: warning: excess elements in scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |                                        ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:40: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:43: warning: excess elements in scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |                                           ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:43: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:46: warning: excess elements in scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |                                              ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:46: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:49: warning: excess elements in scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |                                                 ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:49: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:52: warning: excess elements in scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |                                                    ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:52: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:55: warning: excess elements in scalar initializer
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |                                                       ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:55: note: (near initialization for 'supported_PX[10].firmware_bl_version')
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:679:5: warning: missing initializer for field 'fpga_bit' of 'const struct PX_profile' [-Wmissing-field-initializers]
  679 |     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,{0, 0, 0, 0, 0, 0, 0}}
      |     ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:78:17: note: 'fpga_bit' declared here
   78 |     const char *fpga_bit;
      |                 ^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_scan':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:452:5: warning: this 'else' clause does not guard... [-Wmisleading-indentation]
  452 |     else
      |     ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:455:9: note: ...this statement, but the latter is misleadingly indented as if it were guarded by the 'else'
  455 |         conn = NULL;
      |         ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:445:9: warning: unused variable 'num' [-Wunused-variable]
  445 |     int num = 0;
      |         ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:439:9: warning: unused variable 'devcnt' [-Wunused-variable]
  439 |     int devcnt, ret, i, j;
      |         ^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_dev_mode_list':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:626:35: warning: passing argument 2 of 'g_slist_append' discards 'const' qualifier from pointer target type [-Wdiscarded-qualifiers]
  626 |             l = g_slist_append(l, &sr_mode_list[i]);
      |                                   ^~~~~~~~~~~~~~~~
In file included from D:/msys64/mingw64/include/glib-2.0/glib/gmain.h:28,
                 from D:/msys64/mingw64/include/glib-2.0/glib/giochannel.h:35,
                 from D:/msys64/mingw64/include/glib-2.0/glib.h:56,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.h:24:
D:/msys64/mingw64/include/glib-2.0/glib/gslist.h:61:61: note: expected 'gpointer' {aka 'void *'} but argument is of type 'const struct sr_dev_mode *'
   61 |                                           gpointer          data) G_GNUC_WARN_UNUSED_RESULT;
      |                                           ~~~~~~~~~~~~~~~~~~^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'firmware_config':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:637:9: warning: unused variable 'transferred' [-Wunused-variable]
  637 |     int transferred;
      |         ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:635:9: warning: unused variable 'chunksize' [-Wunused-variable]
  635 |     int chunksize, ret;
      |         ^~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_usb_open':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:753:23: warning: unused variable 'device_count' [-Wunused-variable]
  753 |     int ret, skip, i, device_count;
      |                       ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:753:20: warning: unused variable 'i' [-Wunused-variable]
  753 |     int ret, skip, i, device_count;
      |                    ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:753:14: warning: unused variable 'skip' [-Wunused-variable]
  753 |     int ret, skip, i, device_count;
      |              ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:752:25: warning: variable 'drvc' set but not used [-Wunused-but-set-variable]
  752 |     struct drv_context *drvc;
      |                         ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_dev_open':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:911:30: warning: unused variable 'devc' [-Wunused-variable]
  911 |     struct PX_context *const devc = sdi->priv;
      |                              ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_dev_close':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:962:25: warning: unused variable 'devc' [-Wunused-variable]
  962 |     struct PX_context * devc = sdi->priv;
      |                         ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'config_get':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1036:48: warning: unused parameter 'ch' [-Wunused-parameter]
 1036 |                       const struct sr_channel *ch,
      |                       ~~~~~~~~~~~~~~~~~~~~~~~~~^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'config_set':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1244:14: warning: unused variable 'tmp_u64' [-Wunused-variable]
 1244 |     uint64_t tmp_u64;
      |              ^~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1236:42: warning: unused parameter 'ch' [-Wunused-parameter]
 1236 |                       struct sr_channel *ch,
      |                       ~~~~~~~~~~~~~~~~~~~^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'config_list':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1591:23: warning: comparison of integer expressions of different signedness: 'int' and 'long long unsigned int' [-Wsign-compare]
 1591 |         for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
      |                       ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'resubmit_transfer':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1669:9: warning: unused variable 'i' [-Wunused-variable]
 1669 |     int i = 10;
      |         ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'receive_transfer':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1848:29: warning: comparison of integer expressions of different signedness: 'uint32_t' {aka 'unsigned int'} and 'int' [-Wsign-compare]
 1848 |         if(devc->block_size != transfer->actual_length && devc-> usb_speed != LIBUSB_SPEED_SUPER ){
      |                             ^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1697:14: warning: unused variable 'samples_counter2' [-Wunused-variable]
 1697 |     uint64_t samples_counter2;
      |              ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1696:14: warning: unused variable 'i' [-Wunused-variable]
 1696 |     uint64_t i;
      |              ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:1694:12: warning: unused variable 'samples_elaspsed' [-Wunused-variable]
 1694 |     double samples_elaspsed;
      |            ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'set_trigger':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2062:18: warning: unused variable 'trigger_point' [-Wunused-variable]
 2062 |         uint32_t trigger_point;
      |                  ^~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2059:13: warning: unused variable 'num_trigger_stages' [-Wunused-variable]
 2059 |         int num_trigger_stages = 0;
      |             ^~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2058:28: warning: unused variable 'num_enabled_channels' [-Wunused-variable]
 2058 |         const unsigned int num_enabled_channels = en_ch_num(sdi);
      |                            ^~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2057:21: warning: unused variable 'm' [-Wunused-variable]
 2057 |         uint32_t i, m;
      |                     ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'start_transfers':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2294:37: warning: integer overflow in expression of type 'int' results in '705032704' [-Woverflow]
 2294 |         usb_samples_1s = 5*1000*1000*1000; //5G USB3.0
      |                                     ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2250:14: warning: unused variable 'dma_size_min' [-Wunused-variable]
 2250 |     uint64_t dma_size_min = 4096;
      |              ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2249:14: warning: variable 'dma_size' set but not used [-Wunused-but-set-variable]
 2249 |     uint64_t dma_size = 4096;
      |              ^~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2241:53: warning: variable 'sending_last' set but not used [-Wunused-but-set-variable]
 2241 |     uint64_t samples_to_send = 0, sending_total = 0,sending_last = 0;
      |                                                     ^~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'receive_data2':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2587:14: warning: unused variable 'trigger_pos_real' [-Wunused-variable]
 2587 |     uint32_t trigger_pos_real = 0;
      |              ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2579:14: warning: unused variable 'i' [-Wunused-variable]
 2579 |     uint64_t i;
      |              ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2577:21: warning: unused variable 'last_sample' [-Wunused-variable]
 2577 |     static uint16_t last_sample = 0;
      |                     ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2576:23: warning: unused variable 'elapsed' [-Wunused-variable]
 2576 |         int64_t time, elapsed;
      |                       ^~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2576:17: warning: unused variable 'time' [-Wunused-variable]
 2576 |         int64_t time, elapsed;
      |                 ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2575:35: warning: unused variable 'sending_now' [-Wunused-variable]
 2575 |     uint64_t samples_to_send = 0, sending_now;
      |                                   ^~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2575:14: warning: unused variable 'samples_to_send' [-Wunused-variable]
 2575 |     uint64_t samples_to_send = 0, sending_now;
      |              ^~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2574:12: warning: unused variable 'samples_elaspsed' [-Wunused-variable]
 2574 |     double samples_elaspsed;
      |            ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2573:30: warning: unused variable 'logic' [-Wunused-variable]
 2573 |     struct sr_datafeed_logic logic;
      |                              ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2572:31: warning: unused variable 'packet' [-Wunused-variable]
 2572 |     struct sr_datafeed_packet packet;
      |                               ^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_dev_acquisition_start':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2692:11: warning: unused variable 'rc' [-Wunused-variable]
 2692 |     int i,rc;
      |           ^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2692:9: warning: unused variable 'i' [-Wunused-variable]
 2692 |     int i,rc;
      |         ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2691:34: warning: unused variable 'lupfd' [-Wunused-variable]
 2691 |     const struct libusb_pollfd **lupfd;
      |                                  ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2690:25: warning: variable 'drvc' set but not used [-Wunused-but-set-variable]
 2690 |     struct drv_context *drvc;
      |                         ^~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2689:29: warning: variable 'usb' set but not used [-Wunused-but-set-variable]
 2689 |     struct sr_usb_dev_inst *usb;
      |                             ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'finish_acquisition':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2799:9: warning: unused variable 'ret' [-Wunused-variable]
 2799 |     int ret;
      |         ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2798:14: warning: unused variable 'trigger_pos_real' [-Wunused-variable]
 2798 |     uint32_t trigger_pos_real;
      |              ^~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2796:29: warning: variable 'usb' set but not used [-Wunused-but-set-variable]
 2796 |     struct sr_usb_dev_inst *usb;
      |                             ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'sr_dslogic_option_value_to_code2':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2894:24: warning: comparison of integer expressions of different signedness: 'int' and 'long long unsigned int' [-Wsign-compare]
 2894 |          for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
      |                        ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2900:27: warning: comparison of integer expressions of different signedness: 'int' and 'long long unsigned int' [-Wsign-compare]
 2900 |                     if (i < ARRAY_SIZE(channel_mode_cn_map)){
      |                           ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2901:49: warning: comparison of integer expressions of different signedness: 'enum PX_CHANNEL_ID' and 'int' [-Wsign-compare]
 2901 |                         if (channel_modes[i].id != channel_mode_cn_map[i].id)
      |                                                 ^~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:2883:9: warning: unused variable 'n' [-Wunused-variable]
 2883 |     int n;
      |         ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'firmware_config':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:736:8: warning: 'ret' may be used uninitialized [-Wmaybe-uninitialized]
  736 |     if (ret != SR_OK){
      |        ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:635:20: note: 'ret' was declared here
  635 |     int chunksize, ret;
      |                    ^~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:41:
In function 'hw_usb_open',
    inlined from 'hw_dev_open' at C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:918:5:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/log.h:52:30: warning: 'ret' may be used uninitialized [-Wmaybe-uninitialized]
   52 | #define sr_err(fmt, args...) xlog_err(sr_log, LOG_PREFIX fmt, ## args)
      |                              ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:778:9: note: in expansion of macro 'sr_err'
  778 |         sr_err("Failed to open device: %s, handle:%p",
      |         ^~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c: In function 'hw_dev_open':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/hardware/pxlogic/pxlogic.c:753:9: note: 'ret' was declared here
  753 |     int ret, skip, i, device_count;
      |         ^~~
[504/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/exception.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/exception.c:21:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/exception.c:20:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[506/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/session.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/session.c:22:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/session.c:21:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[507/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/version.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/version.c:21:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/version.c:20:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[508/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/util.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/util.c:22:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/util.c:21:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[509/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/instance.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/instance.c:23:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/instance.c:22:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
[510/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/c_decoder_api.c.obj
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c: In function 'c_decoder_put_python':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:398:38: warning: unused variable 'pda' [-Wunused-variable]
  398 |     struct srd_proto_data_annotation pda;
      |                                      ^~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:397:27: warning: variable 'pdata' set but not used [-Wunused-but-set-variable]
  397 |     struct srd_proto_data pdata;
      |                           ^~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/c_decoder_api.c:396:29: warning: unused variable 'cb' [-Wunused-variable]
  396 |     struct srd_pd_callback *cb;
      |                             ^~
[512/515] Building C object CMakeFiles/PXView.dir/libsigrokdecode/decoder.c.obj
In file included from D:/msys64/mingw64/include/python3.14/Python.h:14,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/libsigrokdecode-internal.h:28,
                 from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/decoder.c:23:
D:/msys64/mingw64/include/python3.14/pyconfig.h:2016:9: warning: '_POSIX_C_SOURCE' redefined
 2016 | #define _POSIX_C_SOURCE 200809L
      |         ^~~~~~~~~~~~~~~
In file included from C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/decoder.c:22:
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/config.h:113:10: note: this is the location of the previous definition
  113 | # define _POSIX_C_SOURCE 200112L
      |          ^~~~~~~~~~~~~~~
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/decoder.c: In function 'srd_c_decoder_load_single':
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/decoder.c:1383:51: warning: cast between incompatible function types from 'FARPROC' {aka 'long long int (*)()'} to 'int (*)(void)' [-Wcast-function-type]
 1383 |     srd_c_decoder_api_version_func version_func = (srd_c_decoder_api_version_func)GetProcAddress(handle, "srd_c_decoder_api_version");
      |                                                   ^
C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokdecode/decoder.c:1384:43: warning: cast between incompatible function types from 'FARPROC' {aka 'long long int (*)()'} to 'struct srd_c_decoder * (*)(void)' [-Wcast-function-type]
 1384 |     srd_c_decoder_entry_func entry_func = (srd_c_decoder_entry_func)GetProcAddress(handle, "srd_c_decoder_entry");
      |                                           ^
[514/515] Install the project...-- Install configuration: "Release"
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/bin/PXView.exe
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCope.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCope.fw
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCope1.def.dsc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCope2.def.dsc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCope20.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCope20.fw
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCopeC20B.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCopeC20P.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCopeU2B100.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCopeU2B20.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCopeU2P20.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSCopeU3P100.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogic.fw
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogic0.def.dsc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogic1.def.dsc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogic2.def.dsc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogic33.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogic50.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicBasic.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicPlus-pgl12-2.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicPlus-pgl12.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicPlus.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicPro.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicPro.fw
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicU2Basic-pgl12-2.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicU2Basic-pgl12.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicU2Basic.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicU2Pro16.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicU3Pro16.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/DSLogicU3Pro32.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/hspi_ddr.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/hspi_ddr_RST.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/license.txt
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/PX_Logic0.def.pxc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/SCI_LOGIC.bin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/virtual-demo1.dsc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/res/virtual-demo1.pxc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/analog
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/analog/sawtooth.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/analog/sine.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/analog/square.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/analog/triangle.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/dso
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/dso/sawtooth.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/dso/sine.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/dso/square.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/dso/triangle.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/logic
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/demo/logic/protocol.demo
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/logo.svg
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/icons/hicolor/scalable/apps/pxview.svg
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/pixmaps/pxview.svg
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/NEWS25
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/NEWS31
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/ug25.pdf
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/ug31.pdf
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-i2c
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-i2c/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-i2c/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-uart
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-uart/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/0-uart/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-i2c
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-i2c/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-i2c/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-uart
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-uart/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/1-uart/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/4b5b
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/4b5b/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/4b5b/symbols.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/4b5b/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/a7105
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/a7105/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/a7105/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ac97
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ac97/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ac97/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5593r
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5593r/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5593r/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5593r/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5626
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5626/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad5626/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad79x0
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad79x0/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ad79x0/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adat
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adat/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adat/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adb
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adb/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adb/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ade77xx
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ade77xx/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ade77xx/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ade77xx/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adf435x
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adf435x/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adf435x/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adns5020
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adns5020/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adns5020/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adxl345
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adxl345/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adxl345/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/adxl345/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/afsk
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/afsk/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/afsk/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/am230x
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/am230x/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/am230x/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/amulet_ascii
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/amulet_ascii/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/amulet_ascii/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/amulet_ascii/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_etmv3
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_etmv3/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_etmv3/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_itm
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_itm/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_itm/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_tpiu
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_tpiu/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arm_tpiu/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arp/dicts.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/arp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/as5047
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/as5047/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/as5047/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/atsha204a
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/atsha204a/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/atsha204a/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/aud
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/aud/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/aud/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avclan
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avclan/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avclan/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avclan/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_isp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_isp/parts.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_isp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_isp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_pdi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_pdi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/avr_pdi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bean
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bean/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bean/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bean/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bh1750
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bh1750/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bh1750/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bluetooth_h4
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bluetooth_h4/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/bluetooth_h4/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/boost
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/boost/handlers.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/boost/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/boost/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/c2
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/c2/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/c2/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/caliper
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/caliper/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/caliper/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/can
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/can/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/can/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/can-fd
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/can-fd/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/can-fd/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/carrera
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/carrera/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/carrera/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cc1101
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cc1101/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cc1101/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cc1101/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ccd
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ccd/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ccd/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cec
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cec/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cec/protocoldata.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cec/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cfp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cfp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cfp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cjtag
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cjtag/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cjtag/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cjtag-oscan0
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cjtag-oscan0/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cjtag-oscan0/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/plugtrx
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/plugtrx/mod.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/plugtrx/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/sdcard
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/sdcard/mod.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/sdcard/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/srdhelper
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/srdhelper/mod.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/srdhelper/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/common/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/counter
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/counter/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/counter/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/crsf
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/crsf/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/crsf/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cyrf6936
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cyrf6936/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cyrf6936/regdecode.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cyrf6936/regs.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/cyrf6936/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dali
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dali/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dali/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dali/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dcc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dcc/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dcc/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dcf77
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dcf77/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dcf77/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/delta-sigma
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/delta-sigma/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/delta-sigma/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dmx512
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dmx512/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dmx512/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds1307
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds1307/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds1307/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds2408
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds2408/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds2408/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds243x
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds243x/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds243x/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds28ea00
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds28ea00/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds28ea00/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds3231
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds3231/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ds3231/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dsi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dsi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/dsi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/edid
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/edid/config
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/edid/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/edid/pnpids.txt
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/edid/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom24xx
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom24xx/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom24xx/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom24xx/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom93xx
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom93xx/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eeprom93xx/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/em4100
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/em4100/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/em4100/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/em4305
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/em4305/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/em4305/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/emmc_sd
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/emmc_sd/mod.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/emmc_sd/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/emmc_sd/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/enc28j60
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/enc28j60/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/enc28j60/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/enc28j60/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ethernet
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ethernet/dicts.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ethernet/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ethernet/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eth_an
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eth_an/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/eth_an/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/example
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/example/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/example/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/flexray
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/flexray/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/flexray/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/fsi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/fsi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/fsi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/gpib
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/gpib/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/gpib/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/graycode
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/graycode/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/graycode/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/guess_bitrate
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/guess_bitrate/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/guess_bitrate/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdcp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdcp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdcp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdlc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdlc/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdlc/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdmi_scdc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdmi_scdc/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/hdmi_scdc/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2c
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2c/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2c/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2cdemux
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2cdemux/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2cdemux/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2cfilter
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2cfilter/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2cfilter/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2c_packet
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2c_packet/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2c_packet/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2s
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2s/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/i2s/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iebus
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iebus/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iebus/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iec
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iec/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iec/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ieee488
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ieee488/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ieee488/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ipv4
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ipv4/dicts.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ipv4/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ipv4/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_irmp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_irmp/irmp_library.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_irmp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_irmp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_ltto
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_ltto/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_ltto/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_ltto_decode
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_ltto_decode/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_ltto_decode/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_nec
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_nec/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_nec/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_nec/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc5
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc5/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc5/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc5/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc6
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc6/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_rc6/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_recoil
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_recoil/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_recoil/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_sirc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_sirc/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_sirc/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ir_sirc/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iso7816
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iso7816/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/iso7816/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/j1708
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/j1708/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/j1708/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jitter
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jitter/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jitter/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_avr
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_avr/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_avr/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_ejtag
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_ejtag/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_ejtag/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_stm32
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_stm32/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/jtag_stm32/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lfast
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lfast/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lfast/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lin
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lin/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lin/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lm75
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lm75/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lm75/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lpc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lpc/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/lpc/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltar_smartdevice
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltar_smartdevice/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltar_smartdevice/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltar_smartdevice_decode
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltar_smartdevice_decode/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltar_smartdevice_decode/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltc242x
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltc242x/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltc242x/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltc26x7
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltc26x7/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ltc26x7/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/maple_bus
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/maple_bus/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/maple_bus/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/max6954
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/max6954/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/max6954/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/max7219
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/max7219/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/max7219/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mcs48
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mcs48/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mcs48/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mdio
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mdio/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mdio/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/microwire
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/microwire/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/microwire/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/midi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/midi/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/midi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/midi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/miller
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/miller/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/miller/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mipi_dsi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mipi_dsi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mipi_dsi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mipi_rffe
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mipi_rffe/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mipi_rffe/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mlx90614
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mlx90614/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mlx90614/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/modbus
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/modbus/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/modbus/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/morse
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/morse/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/morse/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mpu6050
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mpu6050/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mpu6050/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mrf24j40
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mrf24j40/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mrf24j40/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mrf24j40/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mvb
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mvb/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mvb/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mxc6225xu
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mxc6225xu/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/mxc6225xu/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nes_gamepad
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nes_gamepad/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nes_gamepad/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrf24l01
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrf24l01/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrf24l01/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrf905
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrf905/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrf905/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrzi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrzi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nrzi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/numbers_and_state
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/numbers_and_state/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/numbers_and_state/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nunchuk
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nunchuk/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/nunchuk/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/onewire_link
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/onewire_link/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/onewire_link/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/onewire_network
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/onewire_network/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/onewire_network/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/one_single_wire
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/one_single_wire/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/one_single_wire/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_oregon
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_oregon/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_oregon/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_oregon/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_vis
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_vis/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ook_vis/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/opentherm
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/opentherm/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/opentherm/otdecoder.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/opentherm/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/opentherm/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pan1321
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pan1321/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pan1321/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/parallel
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/parallel/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/parallel/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pca9571
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pca9571/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pca9571/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pcfx-ctrlr
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pcfx-ctrlr/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pcfx-ctrlr/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pjdl
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pjdl/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pjdl/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pjon
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pjon/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pjon/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pn532
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pn532/list.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pn532/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pn532/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_keyboard
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_keyboard/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_keyboard/sc.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_keyboard/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_mouse
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_mouse/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ps2_mouse/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pwm
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pwm/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pwm/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pxx1
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pxx1/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/pxx1/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/qi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/qi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/qi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/qspi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/qspi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/qspi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rc_encode
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rc_encode/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rc_encode/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rfm12
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rfm12/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rfm12/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rgb_led_spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rgb_led_spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rgb_led_spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rgb_led_ws281x
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rgb_led_ws281x/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rgb_led_ws281x/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rinnai-control-panel
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rinnai-control-panel/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rinnai-control-panel/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rpm
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rpm/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rpm/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rtc8564
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rtc8564/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rtc8564/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rvswd
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rvswd/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/rvswd/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sae_j1850_vpw
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sae_j1850_vpw/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sae_j1850_vpw/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sbus_futaba
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sbus_futaba/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sbus_futaba/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/scs
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/scs/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/scs/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sda2506
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sda2506/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sda2506/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdcard_sd
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdcard_sd/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdcard_sd/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdcard_spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdcard_spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdcard_spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdio
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdio/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdio/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdio/sd_crc.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdio/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdq
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdq/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sdq/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sent
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sent/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sent/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/seven_segment
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/seven_segment/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/seven_segment/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/signature
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/signature/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/signature/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sipi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sipi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sipi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sle44xx
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sle44xx/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sle44xx/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sony_md
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sony_md/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sony_md/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sony_md_decode
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sony_md_decode/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/sony_md_decode/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spacewire
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spacewire/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spacewire/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spdif
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spdif/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spdif/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi-fast
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi-fast/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi-fast/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spiflash
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spiflash/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spiflash/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spiflash/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_dual_quad
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_dual_quad/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_dual_quad/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_tpm
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_tpm/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_tpm/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_tpm/RangeDict.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_tpm/README.md
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/spi_tpm/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ssd1306
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ssd1306/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ssd1306/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ssi32
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ssi32/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ssi32/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25dv
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25dv/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25dv/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25r39xx_spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25r39xx_spi/lists.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25r39xx_spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st25r39xx_spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st7735
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st7735/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st7735/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st7789
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st7789/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/st7789/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/stepper_motor
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/stepper_motor/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/stepper_motor/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/streletz
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/streletz/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/streletz/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/subfolders_list.txt
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swd
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swd/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swd/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swim
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swim/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/swim/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/t55xx
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/t55xx/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/t55xx/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tca6408a
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tca6408a/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tca6408a/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tcs3472x
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tcs3472x/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tcs3472x/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tdm_audio
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tdm_audio/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tdm_audio/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/timing
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/timing/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/timing/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tlc5620
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tlc5620/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tlc5620/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tm1637
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tm1637/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tm1637/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tm1638
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tm1638/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tm1638/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tmc
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tmc/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tmc/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tmp102
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tmp102/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tmp102/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_fifo_tis
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_fifo_tis/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_fifo_tis/tpm_fifo_tis.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_fifo_tis/tpm_tis_registers.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_fifo_tis/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_i2c
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_i2c/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_i2c/tpm_tis_i2c.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_i2c/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_spi
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_spi/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_spi/tpm_tis_spi.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/tpm_tis_spi/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/uart
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/uart/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/uart/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/uart-fast
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/uart-fast/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/uart-fast/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/udp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/udp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/udp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ufcs
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ufcs/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/ufcs/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_packet
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_packet/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_packet/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_power_delivery
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_power_delivery/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_power_delivery/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_request
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_request/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_request/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_signalling
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_signalling/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/usb_signalling/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/wiegand
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/wiegand/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/wiegand/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/x2444m
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/x2444m/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/x2444m/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/xfp
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/xfp/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/xfp/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/xy2-100
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/xy2-100/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/xy2-100/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/z80
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/z80/pd.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/z80/tables.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/z80/__init__.py
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/decoders/文件夹.bat
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libspi_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libi2c_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libuart_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libcan_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libjtag_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libswd_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libonewire_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libi2s_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/liblin_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libhdlc_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libmicrowire_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libmdio_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libps2_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libdmx512_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libnrzi_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libir_nec_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libir_rc5_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libdcf77_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libcec_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libspdif_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libusb_signalling_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/lib4b5b_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libcan_fd_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libiso7816_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/liblpc_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libdali_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libc2_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libgraycode_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libcounter_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/liblm75_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libds1307_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libds3231_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libnumbers_and_state_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libseven_segment_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libpwm_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libwiegand_c.dll
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/libsigrokdecode/c_decoders/libir_sirc_c.dll
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec/0.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec/a.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec/f.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec/k.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec/p.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dec/u.json
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dlg.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dsl_channel.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dsl_label.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/dsl_list.json
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/msg.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/cn/toolbar.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/en
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/en/dlg.json
-- Installing: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/en/msg.json
-- Up-to-date: C:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/install-qt6.dir/share/PXView/lang/en/toolbar.json

