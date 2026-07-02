#===============================================================================
#= Compat drivers — definitions + serial/ftdi deps
#-------------------------------------------------------------------------------

if(ENABLE_COMPAT_DRIVERS)
    add_definitions(-DHAVE_COMPAT_DRIVERS)

    # Find libserialport for serial compat layer
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSERIALPORT QUIET libserialport)
    endif()
    if(LIBSERIALPORT_FOUND)
        add_definitions(-DHAVE_LIBSERIALPORT)
        include_directories(${LIBSERIALPORT_INCLUDE_DIRS})
    endif()

    # Find libftdi1 for FTDI-based compat drivers (ftdi-la, chronovu-la, ikalogic-scanaplus, pipistrello-ols)
    if(ENABLE_DRIVER_FTDI_LA OR ENABLE_DRIVER_CHRONOVU_LA OR ENABLE_DRIVER_IKALOGIC_SCANAPLUS OR ENABLE_DRIVER_PIPISTRELLO_OLS)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBFTDI1 libftdi1)
        endif()
        if(NOT LIBFTDI1_FOUND)
            message(FATAL_ERROR "libftdi1 is required by the ftdi-la/chronovu-la/ikalogic-scanaplus/pipistrello-ols drivers but was not found. Disable ENABLE_DRIVER_FTDI_LA/ENABLE_DRIVER_CHRONOVU_LA/ENABLE_DRIVER_IKALOGIC_SCANAPLUS/ENABLE_DRIVER_PIPISTRELLO_OLS or install libftdi1.")
        endif()
        include_directories(${LIBFTDI1_INCLUDE_DIRS})
    endif()

    if(ENABLE_DRIVER_FX2LAFW)
        add_definitions(-DHAVE_DRIVER_FX2LAFW)
    endif()
    if(ENABLE_DRIVER_SALEAE_LOGIC16)
        add_definitions(-DHAVE_DRIVER_SALEAE_LOGIC16)
    endif()
    if(ENABLE_DRIVER_SALEAE_LOGIC_PRO)
        add_definitions(-DHAVE_DRIVER_SALEAE_LOGIC_PRO)
    endif()
    if(ENABLE_DRIVER_RASPBERRYPI_PICO)
        add_definitions(-DHAVE_DRIVER_RASPBERRYPI_PICO)
    endif()
    if(ENABLE_DRIVER_ASIX_SIGMA)
        add_definitions(-DHAVE_DRIVER_ASIX_SIGMA)
    endif()
    if(ENABLE_DRIVER_CHRONOVU_LA)
        add_definitions(-DHAVE_DRIVER_CHRONOVU_LA)
    endif()
    if(ENABLE_DRIVER_FTDI_LA)
        add_definitions(-DHAVE_DRIVER_FTDI_LA)
    endif()
    if(ENABLE_DRIVER_KINGST_LA2016)
        add_definitions(-DHAVE_DRIVER_KINGST_LA2016)
    endif()
    if(ENABLE_DRIVER_SYSCLK_LWLA)
        add_definitions(-DHAVE_DRIVER_SYSCLK_LWLA)
    endif()
    if(ENABLE_DRIVER_SYSCLK_SLA5032)
        add_definitions(-DHAVE_DRIVER_SYSCLK_SLA5032)
    endif()
    if(ENABLE_DRIVER_FLUKE_DMM)
        add_definitions(-DHAVE_DRIVER_FLUKE_DMM)
    endif()
    if(ENABLE_DRIVER_AGILENT_DMM)
        add_definitions(-DHAVE_DRIVER_AGILENT_DMM)
    endif()
    if(ENABLE_DRIVER_NORMA_DMM)
        add_definitions(-DHAVE_DRIVER_NORMA_DMM)
    endif()
    if(ENABLE_DRIVER_SERIAL_DMM)
        add_definitions(-DHAVE_DRIVER_SERIAL_DMM)
    endif()
    if(ENABLE_DRIVER_APPA_55II)
        add_definitions(-DHAVE_DRIVER_APPA_55II)
    endif()
    if(ENABLE_DRIVER_FLUKE_45)
        add_definitions(-DHAVE_DRIVER_FLUKE_45)
    endif()
    if(ENABLE_DRIVER_RIGOL_DS)
        add_definitions(-DHAVE_DRIVER_RIGOL_DS)
    endif()
    if(ENABLE_DRIVER_ROHDE_SCHWARZ_SME_0X)
        add_definitions(-DHAVE_DRIVER_ROHDE_SCHWARZ_SME_0X)
    endif()
    if(ENABLE_DRIVER_SIGLENT_SDS)
        add_definitions(-DHAVE_DRIVER_SIGLENT_SDS)
    endif()
    if(ENABLE_DRIVER_RIGOL_DG)
        add_definitions(-DHAVE_DRIVER_RIGOL_DG)
    endif()
    if(ENABLE_DRIVER_OPENBENCH_LOGIC_SNIFFER)
        add_definitions(-DHAVE_DRIVER_OPENBENCH_LOGIC_SNIFFER)
    endif()
    if(ENABLE_DRIVER_IKALOGIC_SCANAPLUS)
        add_definitions(-DHAVE_DRIVER_IKALOGIC_SCANAPLUS)
    endif()
    if(ENABLE_DRIVER_IKALOGIC_SCANALOGIC2)
        add_definitions(-DHAVE_DRIVER_IKALOGIC_SCANALOGIC2)
    endif()
    if(ENABLE_DRIVER_LECROY_LOGICSTUDIO)
        add_definitions(-DHAVE_DRIVER_LECROY_LOGICSTUDIO)
    endif()
    if(ENABLE_DRIVER_IPDBG_LA)
        add_definitions(-DHAVE_DRIVER_IPDBG_LA)
    endif()
    if(ENABLE_DRIVER_PIPISTRELLO_OLS)
        add_definitions(-DHAVE_DRIVER_PIPISTRELLO_OLS)
    endif()
    if(ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER)
        add_definitions(-DHAVE_DRIVER_SIPEED_SLOGIC_ANALYZER)
    endif()
    if(ENABLE_DRIVER_HANTEK_6XXX)
        add_definitions(-DHAVE_DRIVER_HANTEK_6XXX)
    endif()
    if(ENABLE_DRIVER_HANTEK_4032L)
        add_definitions(-DHAVE_DRIVER_HANTEK_4032L)
    endif()
    if(ENABLE_DRIVER_HANTEK_DSO)
        add_definitions(-DHAVE_DRIVER_HANTEK_DSO)
    endif()
    if(ENABLE_DRIVER_GWINSTEK_GDS_800)
        add_definitions(-DHAVE_DRIVER_GWINSTEK_GDS_800)
    endif()
    if(ENABLE_DRIVER_GWINSTEK_PSP)
        add_definitions(-DHAVE_DRIVER_GWINSTEK_PSP)
    endif()
    if(ENABLE_DRIVER_KORAD_KAXXXXP)
        add_definitions(-DHAVE_DRIVER_KORAD_KAXXXXP)
    endif()
    if(ENABLE_DRIVER_ATTEN_PPS3XXX)
        add_definitions(-DHAVE_DRIVER_ATTEN_PPS3XXX)
    endif()
    if(ENABLE_DRIVER_MANSON_HCS_3XXX)
        add_definitions(-DHAVE_DRIVER_MANSON_HCS_3XXX)
    endif()
    if(ENABLE_DRIVER_MOTECH_LPS_30X)
        add_definitions(-DHAVE_DRIVER_MOTECH_LPS_30X)
    endif()
    if(ENABLE_DRIVER_RDTECH_DPS)
        add_definitions(-DHAVE_DRIVER_RDTECH_DPS)
    endif()
    if(ENABLE_DRIVER_RDTECH_TC)
        add_definitions(-DHAVE_DRIVER_RDTECH_TC)
    endif()
    if(ENABLE_DRIVER_RDTECH_UM)
        add_definitions(-DHAVE_DRIVER_RDTECH_UM)
    endif()
    if(ENABLE_DRIVER_ITECH_IT8500)
        add_definitions(-DHAVE_DRIVER_ITECH_IT8500)
    endif()
    if(ENABLE_DRIVER_MAYNUO_M97)
        add_definitions(-DHAVE_DRIVER_MAYNUO_M97)
    endif()
    if(ENABLE_DRIVER_SIGLENT_SDL10X0)
        add_definitions(-DHAVE_DRIVER_SIGLENT_SDL10X0)
    endif()
    if(ENABLE_DRIVER_SCPI_PPS)
        add_definitions(-DHAVE_DRIVER_SCPI_PPS)
    endif()
    if(ENABLE_DRIVER_LECROY_XSTREAM)
        add_definitions(-DHAVE_DRIVER_LECROY_XSTREAM)
    endif()
    if(ENABLE_DRIVER_HAMEG_HMO)
        add_definitions(-DHAVE_DRIVER_HAMEG_HMO)
    endif()
    if(ENABLE_DRIVER_UNI_T_UT181A)
        add_definitions(-DHAVE_DRIVER_UNI_T_UT181A)
    endif()
    if(ENABLE_DRIVER_UNI_T_UT32X)
        add_definitions(-DHAVE_DRIVER_UNI_T_UT32X)
    endif()
    if(ENABLE_DRIVER_UNI_T_DMM)
        add_definitions(-DHAVE_DRIVER_UNI_T_DMM)
    endif()
    if(ENABLE_DRIVER_LINK_MSO19)
        add_definitions(-DHAVE_DRIVER_LINK_MSO19)
    endif()
    if(ENABLE_DRIVER_MASTECH_MS6514)
        add_definitions(-DHAVE_DRIVER_MASTECH_MS6514)
    endif()
    if(ENABLE_DRIVER_TESTO)
        add_definitions(-DHAVE_DRIVER_TESTO)
    endif()
    if(ENABLE_DRIVER_LASCAR_EL_USB)
        add_definitions(-DHAVE_DRIVER_LASCAR_EL_USB)
    endif()
    if(ENABLE_DRIVER_TONDAJ_SL_814)
        add_definitions(-DHAVE_DRIVER_TONDAJ_SL_814)
    endif()
    if(ENABLE_DRIVER_PCE_322A)
        add_definitions(-DHAVE_DRIVER_PCE_322A)
    endif()
    if(ENABLE_DRIVER_CENTER_3XX)
        add_definitions(-DHAVE_DRIVER_CENTER_3XX)
    endif()
    if(ENABLE_DRIVER_MIC_985XX)
        add_definitions(-DHAVE_DRIVER_MIC_985XX)
    endif()
    if(ENABLE_DRIVER_TELEINFO)
        add_definitions(-DHAVE_DRIVER_TELEINFO)
    endif()
    if(ENABLE_DRIVER_KERN_SCALE)
        add_definitions(-DHAVE_DRIVER_KERN_SCALE)
    endif()
    if(ENABLE_DRIVER_CONRAD_DIGI_35_CPU)
        add_definitions(-DHAVE_DRIVER_CONRAD_DIGI_35_CPU)
    endif()
    if(ENABLE_DRIVER_HP_59306A)
        add_definitions(-DHAVE_DRIVER_HP_59306A)
    endif()
    if(ENABLE_DRIVER_COLEAD_SLM)
        add_definitions(-DHAVE_DRIVER_COLEAD_SLM)
    endif()
    if(ENABLE_DRIVER_ICSTATION_USBRELAY)
        add_definitions(-DHAVE_DRIVER_ICSTATION_USBRELAY)
    endif()
    if(ENABLE_DRIVER_ZKETECH_EBD_USB)
        add_definitions(-DHAVE_DRIVER_ZKETECH_EBD_USB)
    endif()
    if(ENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO)
        add_definitions(-DHAVE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO)
    endif()
    if(ENABLE_DRIVER_ASIX_OMEGA_RTM_CLI)
        add_definitions(-DHAVE_DRIVER_ASIX_OMEGA_RTM_CLI)
    endif()
    if(ENABLE_DRIVER_KECHENG_KC_330B)
        add_definitions(-DHAVE_DRIVER_KECHENG_KC_330B)
    endif()
    if(ENABLE_DRIVER_HP_3457A)
        add_definitions(-DHAVE_DRIVER_HP_3457A)
    endif()
    if(ENABLE_DRIVER_MICROCHIP_PICKIT2)
        add_definitions(-DHAVE_DRIVER_MICROCHIP_PICKIT2)
    endif()
    if(ENABLE_DRIVER_HP_3478A)
        add_definitions(-DHAVE_DRIVER_HP_3478A)
    endif()
    if(ENABLE_DRIVER_CEM_DT_885X)
        add_definitions(-DHAVE_DRIVER_CEM_DT_885X)
    endif()
    if(ENABLE_DRIVER_ATORCH)
        add_definitions(-DHAVE_DRIVER_ATORCH)
    endif()
    if(ENABLE_DRIVER_BKPRECISION_1856D)
        add_definitions(-DHAVE_DRIVER_BKPRECISION_1856D)
    endif()
    if(ENABLE_DRIVER_GWINSTEK_GPD)
        add_definitions(-DHAVE_DRIVER_GWINSTEK_GPD)
    endif()
    if(ENABLE_DRIVER_SCPI_DMM)
        add_definitions(-DHAVE_DRIVER_SCPI_DMM)
    endif()
    if(ENABLE_DRIVER_SERIAL_LCR)
        add_definitions(-DHAVE_DRIVER_SERIAL_LCR)
    endif()
    if(ENABLE_DRIVER_JUNTEK_JDS6600)
        add_definitions(-DHAVE_DRIVER_JUNTEK_JDS6600)
    endif()
    if(ENABLE_DRIVER_GMC_MH_1X_2X)
        add_definitions(-DHAVE_DRIVER_GMC_MH_1X_2X)
        add_definitions(-DHAVE_DRIVER_GMC_MH_2X_BD232)
    endif()
    if(ENABLE_DRIVER_GREATFET)
        add_definitions(-DHAVE_DRIVER_GREATFET)
    endif()
    if(ENABLE_DRIVER_DCTTECH_USBRELAY)
        add_definitions(-DHAVE_DRIVER_DCTTECH_USBRELAY)
    endif()
    if(ENABLE_DRIVER_DEVANTECH_ETH008)
        add_definitions(-DHAVE_DRIVER_DEVANTECH_ETH008)
    endif()
endif()

#===============================================================================
#= libsigrok source
#-------------------------------------------------------------------------------
set(libsigrok_SOURCES
    libsigrok/version.c
    libsigrok/strutil.c
    libsigrok/std.c
    libsigrok/session_driver.c
    libsigrok/session.c
    libsigrok/log.c
    libsigrok/hwdriver.c
    libsigrok/error.c
    libsigrok/backend.c
    libsigrok/output/output.c
    libsigrok/input/input.c
    libsigrok/hardware/demo/demo.c
    libsigrok/input/in_binary.c
    libsigrok/input/in_vcd.c
    libsigrok/input/in_wav.c
    libsigrok/output/csv.c
    libsigrok/output/gnuplot.c
    libsigrok/output/srzip.c
    libsigrok/output/vcd.c
    libsigrok/hardware/DSL/dslogic.c
    libsigrok/hardware/common/usb.c
    libsigrok/hardware/common/ezusb.c
    libsigrok/trigger.c
    libsigrok/dsdevice.c
    libsigrok/hardware/DSL/dscope.c
    libsigrok/hardware/DSL/command.c
    libsigrok/hardware/DSL/dsl.c
    libsigrok/hardware/pxlogic/pxlogic.c
    libsigrok/hardware/pxlogic/usb_ctrl.c
    libsigrok/lib_main.c
)

if(ENABLE_COMPAT_DRIVERS)
    list(APPEND libsigrok_SOURCES
        libsigrok/hardware/compat/compat_helpers.c
        libsigrok/hardware/compat/compat_serial.c
        libsigrok/hardware/compat/compat_scpi.c
    )
    if(ENABLE_DRIVER_FX2LAFW)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/fx2lafw/api.c
            libsigrok/hardware/fx2lafw/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SALEAE_LOGIC16)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/saleae-logic16/api.c
            libsigrok/hardware/saleae-logic16/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SALEAE_LOGIC_PRO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/saleae-logic-pro/api.c
            libsigrok/hardware/saleae-logic-pro/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_RASPBERRYPI_PICO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/raspberrypi-pico/api.c
            libsigrok/hardware/raspberrypi-pico/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ASIX_SIGMA)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/asix-sigma/api.c
            libsigrok/hardware/asix-sigma/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_CHRONOVU_LA)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/chronovu-la/api.c
            libsigrok/hardware/chronovu-la/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_FTDI_LA)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/ftdi-la/api.c
            libsigrok/hardware/ftdi-la/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_KINGST_LA2016)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/kingst-la2016/api.c
            libsigrok/hardware/kingst-la2016/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SYSCLK_LWLA)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/sysclk-lwla/api.c
            libsigrok/hardware/sysclk-lwla/protocol.c
            libsigrok/hardware/sysclk-lwla/lwla.c
            libsigrok/hardware/sysclk-lwla/lwla1016.c
            libsigrok/hardware/sysclk-lwla/lwla1034.c
        )
    endif()
    if(ENABLE_DRIVER_SYSCLK_SLA5032)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/sysclk-sla5032/api.c
            libsigrok/hardware/sysclk-sla5032/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ZEROPLUS_LOGIC_CUBE)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/zeroplus-logic-cube/api.c
            libsigrok/hardware/zeroplus-logic-cube/protocol.c
            libsigrok/hardware/zeroplus-logic-cube/analyzer.c
            libsigrok/hardware/zeroplus-logic-cube/gl_usb.c
        )
    endif()
    if(ENABLE_DRIVER_FLUKE_DMM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/fluke-dmm/api.c
            libsigrok/hardware/fluke-dmm/protocol.c
            libsigrok/hardware/fluke-dmm/fluke-18x.c
            libsigrok/hardware/fluke-dmm/fluke-28x.c
            libsigrok/hardware/fluke-dmm/fluke-190.c
        )
    endif()
    if(ENABLE_DRIVER_AGILENT_DMM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/agilent-dmm/api.c
            libsigrok/hardware/agilent-dmm/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_NORMA_DMM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/norma-dmm/api.c
            libsigrok/hardware/norma-dmm/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SERIAL_DMM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/serial-dmm/api.c
            libsigrok/hardware/serial-dmm/protocol.c
            libsigrok/hardware/serial-dmm/asycii.c
            libsigrok/hardware/serial-dmm/bm25x.c
            libsigrok/hardware/serial-dmm/bm52x.c
            libsigrok/hardware/serial-dmm/bm85x.c
            libsigrok/hardware/serial-dmm/bm86x.c
            libsigrok/hardware/serial-dmm/dtm0660.c
            libsigrok/hardware/serial-dmm/eev121gw.c
            libsigrok/hardware/serial-dmm/es519xx.c
            libsigrok/hardware/serial-dmm/fs9721.c
            libsigrok/hardware/serial-dmm/fs9922.c
            libsigrok/hardware/serial-dmm/m2110.c
            libsigrok/hardware/serial-dmm/metex14.c
            libsigrok/hardware/serial-dmm/mm38xr.c
            libsigrok/hardware/serial-dmm/ms2115b.c
            libsigrok/hardware/serial-dmm/ms8250d.c
            libsigrok/hardware/serial-dmm/qm1578.c
            libsigrok/hardware/serial-dmm/rs9lcd.c
            libsigrok/hardware/serial-dmm/ut71x.c
            libsigrok/hardware/serial-dmm/vc870.c
            libsigrok/hardware/serial-dmm/vc96.c
        )
    endif()
    if(ENABLE_DRIVER_FLUKE_45)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/fluke-45/api.c
            libsigrok/hardware/fluke-45/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_APPA_55II)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/appa-55ii/api.c
            libsigrok/hardware/appa-55ii/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_RIGOL_DS)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/rigol-ds/api.c
            libsigrok/hardware/rigol-ds/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ROHDE_SCHWARZ_SME_0X)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/rohde-schwarz-sme-0x/api.c
            libsigrok/hardware/rohde-schwarz-sme-0x/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SIGLENT_SDS)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/siglent-sds/api.c
            libsigrok/hardware/siglent-sds/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_RIGOL_DG)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/rigol-dg/api.c
            libsigrok/hardware/rigol-dg/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_OPENBENCH_LOGIC_SNIFFER)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/openbench-logic-sniffer/api.c
            libsigrok/hardware/openbench-logic-sniffer/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_IKALOGIC_SCANAPLUS)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/ikalogic-scanaplus/api.c
            libsigrok/hardware/ikalogic-scanaplus/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_IPDBG_LA)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/ipdbg-la/api.c
            libsigrok/hardware/ipdbg-la/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_PIPISTRELLO_OLS)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/pipistrello-ols/api.c
            libsigrok/hardware/pipistrello-ols/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/sipeed-slogic-analyzer/api.c
            libsigrok/hardware/sipeed-slogic-analyzer/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_IKALOGIC_SCANALOGIC2)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/ikalogic-scanalogic2/api.c
            libsigrok/hardware/ikalogic-scanalogic2/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_LECROY_LOGICSTUDIO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/lecroy-logicstudio/api.c
            libsigrok/hardware/lecroy-logicstudio/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_HANTEK_6XXX)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hantek-6xxx/api.c
            libsigrok/hardware/hantek-6xxx/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_HANTEK_4032L)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hantek-4032l/api.c
            libsigrok/hardware/hantek-4032l/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_HANTEK_DSO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hantek-dso/api.c
            libsigrok/hardware/hantek-dso/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_GWINSTEK_GDS_800)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/gwinstek-gds-800/api.c
            libsigrok/hardware/gwinstek-gds-800/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_GWINSTEK_PSP)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/gwinstek-psp/api.c
            libsigrok/hardware/gwinstek-psp/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_KORAD_KAXXXXP)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/korad-kaxxxxp/api.c
            libsigrok/hardware/korad-kaxxxxp/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ATTEN_PPS3XXX)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/atten-pps3xxx/api.c
            libsigrok/hardware/atten-pps3xxx/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_MANSON_HCS_3XXX)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/manson-hcs-3xxx/api.c
            libsigrok/hardware/manson-hcs-3xxx/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_MOTECH_LPS_30X)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/motech-lps-30x/api.c
            libsigrok/hardware/motech-lps-30x/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_RDTECH_DPS)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/rdtech-dps/api.c
            libsigrok/hardware/rdtech-dps/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_RDTECH_TC)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/rdtech-tc/api.c
            libsigrok/hardware/rdtech-tc/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_RDTECH_UM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/rdtech-um/api.c
            libsigrok/hardware/rdtech-um/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ITECH_IT8500)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/itech-it8500/api.c
            libsigrok/hardware/itech-it8500/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_MAYNUO_M97)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/maynuo-m97/api.c
            libsigrok/hardware/maynuo-m97/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SIGLENT_SDL10X0)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/siglent-sdl10x0/api.c
            libsigrok/hardware/siglent-sdl10x0/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SCPI_PPS)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/scpi-pps/api.c
            libsigrok/hardware/scpi-pps/protocol.c
            libsigrok/hardware/scpi-pps/profiles.c
        )
    endif()
    if(ENABLE_DRIVER_YOKOGAWA_DLM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/yokogawa-dlm/api.c
            libsigrok/hardware/yokogawa-dlm/protocol.c
            libsigrok/hardware/yokogawa-dlm/protocol_wrappers.c
        )
    endif()
    if(ENABLE_DRIVER_HAMEG_HMO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hameg-hmo/api.c
            libsigrok/hardware/hameg-hmo/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_LECROY_XSTREAM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/lecroy-xstream/api.c
            libsigrok/hardware/lecroy-xstream/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_UNI_T_UT181A)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/uni-t-ut181a/api.c
            libsigrok/hardware/uni-t-ut181a/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_UNI_T_UT32X)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/uni-t-ut32x/api.c
            libsigrok/hardware/uni-t-ut32x/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_UNI_T_DMM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/uni-t-dmm/api.c
            libsigrok/hardware/uni-t-dmm/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_LINK_MSO19)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/link-mso19/api.c
            libsigrok/hardware/link-mso19/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_MASTECH_MS6514)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/mastech-ms6514/api.c
            libsigrok/hardware/mastech-ms6514/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_TESTO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/testo/api.c
            libsigrok/hardware/testo/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_LASCAR_EL_USB)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/lascar-el-usb/api.c
            libsigrok/hardware/lascar-el-usb/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_TONDAJ_SL_814)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/tondaj-sl-814/api.c
            libsigrok/hardware/tondaj-sl-814/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_PCE_322A)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/pce-322a/api.c
            libsigrok/hardware/pce-322a/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_CENTER_3XX)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/center-3xx/api.c
            libsigrok/hardware/center-3xx/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_MIC_985XX)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/mic-985xx/api.c
            libsigrok/hardware/mic-985xx/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_TELEINFO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/teleinfo/api.c
            libsigrok/hardware/teleinfo/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_KERN_SCALE)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/kern-scale/api.c
            libsigrok/hardware/kern-scale/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_CONRAD_DIGI_35_CPU)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/conrad-digi-35-cpu/api.c
            libsigrok/hardware/conrad-digi-35-cpu/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_HP_59306A)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hp-59306a/api.c
            libsigrok/hardware/hp-59306a/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_COLEAD_SLM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/colead-slm/api.c
            libsigrok/hardware/colead-slm/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ICSTATION_USBRELAY)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/icstation-usbrelay/api.c
            libsigrok/hardware/icstation-usbrelay/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ZKETECH_EBD_USB)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/zketech-ebd-usb/api.c
            libsigrok/hardware/zketech-ebd-usb/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/arachnid-labs-re-load-pro/api.c
            libsigrok/hardware/arachnid-labs-re-load-pro/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ASIX_OMEGA_RTM_CLI)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/asix-omega-rtm-cli/api.c
            libsigrok/hardware/asix-omega-rtm-cli/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_KECHENG_KC_330B)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/kecheng-kc-330b/api.c
            libsigrok/hardware/kecheng-kc-330b/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_HP_3457A)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hp-3457a/api.c
            libsigrok/hardware/hp-3457a/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_MICROCHIP_PICKIT2)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/microchip-pickit2/api.c
            libsigrok/hardware/microchip-pickit2/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_HP_3478A)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/hp-3478a/api.c
            libsigrok/hardware/hp-3478a/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_CEM_DT_885X)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/cem-dt-885x/api.c
            libsigrok/hardware/cem-dt-885x/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_ATORCH)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/atorch/api.c
            libsigrok/hardware/atorch/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_BKPRECISION_1856D)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/bkprecision-1856d/api.c
            libsigrok/hardware/bkprecision-1856d/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_GWINSTEK_GPD)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/gwinstek-gpd/api.c
            libsigrok/hardware/gwinstek-gpd/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SCPI_DMM)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/scpi-dmm/api.c
            libsigrok/hardware/scpi-dmm/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_SERIAL_LCR)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/serial-lcr/api.c
            libsigrok/hardware/serial-lcr/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_JUNTEK_JDS6600)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/juntek-jds6600/api.c
            libsigrok/hardware/juntek-jds6600/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_GMC_MH_1X_2X)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/gmc-mh-1x-2x/api.c
            libsigrok/hardware/gmc-mh-1x-2x/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_GREATFET)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/greatfet/api.c
            libsigrok/hardware/greatfet/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_DCTTECH_USBRELAY)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/dcttech-usbrelay/api.c
            libsigrok/hardware/dcttech-usbrelay/protocol.c
        )
    endif()
    if(ENABLE_DRIVER_DEVANTECH_ETH008)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/devantech-eth008/api.c
            libsigrok/hardware/devantech-eth008/protocol.c
        )
    endif()
endif()

set(libsigrok_HEADERS
    libsigrok/version.h
    libsigrok/libsigrok-internal.h
    libsigrok/libsigrok.h
    libsigrok/config.h
    libsigrok/hardware/DSL/command.h
    libsigrok/hardware/DSL/dsl.h
)

#===============================================================================
#= libsigrokdecode source
#-------------------------------------------------------------------------------
set(libsigrokdecode_SOURCES
    libsigrokdecode/type_decoder.c
    libsigrokdecode/srd.c
    libsigrokdecode/module_sigrokdecode.c
    libsigrokdecode/decoder.c
    libsigrokdecode/dll_registry.c
    libsigrokdecode/error.c
    libsigrokdecode/exception.c
    libsigrokdecode/instance.c
    libsigrokdecode/log.c
    libsigrokdecode/session.c
    libsigrokdecode/util.c
    libsigrokdecode/version.c
    libsigrokdecode/c_decoder_api.c
)

set(libsigrokdecode_HEADERS
    libsigrokdecode/libsigrokdecode-internal.h
    libsigrokdecode/libsigrokdecode.h
    libsigrokdecode/config.h
    libsigrokdecode/version.h
)

#===============================================================================
#= common source
#-------------------------------------------------------------------------------

set(common_SOURCES
    common/minizip/zip.c
    common/minizip/unzip.c
    common/minizip/ioapi.c
    common/log/xlog.c
)

set(common_HEADERS
    common/minizip/zip.h
    common/minizip/unzip.h
    common/minizip/ioapi.h
    common/log/xlog.h
)

#===============================================================================
#= Linker Configuration
#-------------------------------------------------------------------------------

set(PXVIEW_LINK_LIBS
	-lz
	-lglib-2.0
	${CMAKE_THREAD_LIBS_INIT}
	${LIBUSB_1_LIBRARIES}
	${FFTW_LIBRARIES}
	${PY_LIB}
)
# Note: Qt libraries are NOT in PXVIEW_LINK_LIBS — they are linked per-target:
#   - pxview-core links ${QT_CORE_LIBS} (no Qt::Widgets, no Qt::Svg)
#   - PXView executable links ${QT_GUI_LIBS} (Qt::Widgets + Qt::Svg)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND PXVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND PXVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

# Add libserialport if found (for serial compat layer)
if(LIBSERIALPORT_FOUND)
    list(APPEND PXVIEW_LINK_LIBS ${LIBSERIALPORT_LIBRARIES})
    link_directories(${LIBSERIALPORT_LIBRARY_DIRS})
endif()

# Add libftdi1 if found (for FTDI-based compat drivers: ftdi-la, chronovu-la)
if(LIBFTDI1_FOUND)
    list(APPEND PXVIEW_LINK_LIBS ${LIBFTDI1_LIBRARIES})
    link_directories(${LIBFTDI1_LIBRARY_DIRS})
endif()
