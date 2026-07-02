#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

set(DISABLE_WERROR TRUE) #Build without -Werror
set(ENABLE_SIGNALS TRUE) #Build with UNIX signals
set(ENABLE_COTIRE FALSE) #Enable cotire
set(ENABLE_TESTS  FALSE) #Enable unit tests
set(STATIC_PKGDEPS_LIBS FALSE) #Statically link to (pkg-config) libraries

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# Windows does not support UNIX signals.
	set(ENABLE_SIGNALS FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

#===============================================================================
#= Compat drivers option
#-------------------------------------------------------------------------------
option(ENABLE_COMPAT_DRIVERS "Enable standard sigrok compatible drivers" OFF)
option(ENABLE_DRIVER_FX2LAFW "Enable fx2lafw driver" ON)
option(ENABLE_DRIVER_SALEAE_LOGIC16 "Enable saleae-logic16 driver" ON)
option(ENABLE_DRIVER_SALEAE_LOGIC_PRO "Enable saleae-logic-pro driver" ON)
option(ENABLE_DRIVER_RASPBERRYPI_PICO "Enable raspberrypi-pico driver" ON)
option(ENABLE_DRIVER_ASIX_SIGMA "Enable asix-sigma driver" ON)
option(ENABLE_DRIVER_CHRONOVU_LA "Enable chronovu-la driver" ON)
option(ENABLE_DRIVER_FTDI_LA "Enable ftdi-la driver" ON)
option(ENABLE_DRIVER_KINGST_LA2016 "Enable kingst-la2016 driver" ON)
option(ENABLE_DRIVER_SYSCLK_LWLA "Enable sysclk-lwla driver" ON)
option(ENABLE_DRIVER_SYSCLK_SLA5032 "Enable sysclk-sla5032 driver" ON)
option(ENABLE_DRIVER_FLUKE_DMM "Enable fluke-dmm driver" ON)
option(ENABLE_DRIVER_AGILENT_DMM "Enable agilent-dmm driver" ON)
option(ENABLE_DRIVER_NORMA_DMM "Enable norma-dmm DMM driver" ON)
option(ENABLE_DRIVER_SERIAL_DMM "Enable serial-dmm driver" ON)
option(ENABLE_DRIVER_APPA_55II "Enable appa-55ii driver" ON)
option(ENABLE_DRIVER_FLUKE_45 "Enable fluke-45 SCPI multimeter driver" ON)
option(ENABLE_DRIVER_RIGOL_DS "Enable rigol-ds SCPI oscilloscope driver" ON)
option(ENABLE_DRIVER_ROHDE_SCHWARZ_SME_0X "Enable rohde-schwarz-sme-0x SCPI signal generator driver" ON)
option(ENABLE_DRIVER_SIGLENT_SDS "Enable siglent-sds SCPI oscilloscope driver" ON)
option(ENABLE_DRIVER_RIGOL_DG "Enable rigol-dg SCPI signal generator driver" ON)
option(ENABLE_DRIVER_OPENBENCH_LOGIC_SNIFFER "Enable openbench-logic-sniffer (OLS) driver" ON)
option(ENABLE_DRIVER_IKALOGIC_SCANAPLUS "Enable ikalogic-scanaplus driver" ON)
option(ENABLE_DRIVER_ZEROPLUS_LOGIC_CUBE "Enable zeroplus-logic-cube driver" ON)
option(ENABLE_DRIVER_IKALOGIC_SCANALOGIC2 "Enable ikalogic-scanalogic2 driver" ON)
option(ENABLE_DRIVER_LECROY_LOGICSTUDIO "Enable lecroy-logicstudio driver" ON)
option(ENABLE_DRIVER_IPDBG_LA "Enable ipdbg-la logic analyzer driver" ON)
option(ENABLE_DRIVER_PIPISTRELLO_OLS "Enable pipistrello-ols logic analyzer (OLS) driver" ON)
option(ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER "Enable sipeed-slogic-analyzer logic analyzer driver" ON)
option(ENABLE_DRIVER_HANTEK_6XXX "Enable hantek-6xxx driver" ON)
option(ENABLE_DRIVER_HANTEK_4032L "Enable hantek-4032l driver" ON)
option(ENABLE_DRIVER_HANTEK_DSO "Enable hantek-dso driver" ON)
option(ENABLE_DRIVER_GWINSTEK_GDS_800 "Enable gwinstek-gds-800 SCPI oscilloscope driver" ON)
option(ENABLE_DRIVER_GWINSTEK_PSP "Enable gwinstek-psp power supply driver" ON)
option(ENABLE_DRIVER_KORAD_KAXXXXP "Enable korad-kaxxxxp power supply driver" ON)
option(ENABLE_DRIVER_ATTEN_PPS3XXX "Enable atten-pps3xxx power supply driver" ON)
option(ENABLE_DRIVER_MANSON_HCS_3XXX "Enable manson-hcs-3xxx power supply driver" ON)
option(ENABLE_DRIVER_MOTECH_LPS_30X "Enable motech-lps-30x power supply driver" ON)
option(ENABLE_DRIVER_RDTECH_DPS "Enable rdtech-dps power supply driver" ON)
option(ENABLE_DRIVER_RDTECH_TC "Enable rdtech-tc USB power meter driver" ON)
option(ENABLE_DRIVER_RDTECH_UM "Enable rdtech-um USB power meter driver" ON)
option(ENABLE_DRIVER_ITECH_IT8500 "Enable itech-it8500 electronic load driver" ON)
option(ENABLE_DRIVER_MAYNUO_M97 "Enable maynuo-m97 electronic load driver" ON)
option(ENABLE_DRIVER_SIGLENT_SDL10X0 "Enable siglent-sdl10x0 electronic load driver" ON)
option(ENABLE_DRIVER_SCPI_PPS "Enable scpi-pps generic SCPI power supply driver" ON)
option(ENABLE_DRIVER_LECROY_XSTREAM "Enable lecroy-xstream SCPI oscilloscope driver" ON)
option(ENABLE_DRIVER_HAMEG_HMO "Enable hameg-hmo SCPI oscilloscope driver" ON)
option(ENABLE_DRIVER_YOKOGAWA_DLM "Enable yokogawa-dlm SCPI oscilloscope driver" ON)
option(ENABLE_DRIVER_UNI_T_UT181A "Enable uni-t-ut181a DMM driver" ON)
option(ENABLE_DRIVER_UNI_T_UT32X "Enable uni-t-ut32x thermometer driver" ON)
option(ENABLE_DRIVER_UNI_T_DMM "Enable uni-t-dmm USB-HID DMM driver" ON)
option(ENABLE_DRIVER_LINK_MSO19 "Enable link-mso19 mixed-signal oscilloscope driver" ON)
option(ENABLE_DRIVER_MASTECH_MS6514 "Enable mastech-ms6514 serial thermometer driver" ON)
option(ENABLE_DRIVER_TESTO "Enable testo serial thermometer/multimeter driver" ON)
option(ENABLE_DRIVER_LASCAR_EL_USB "Enable lascar-el-usb USB temperature/humidity/CO data logger driver" ON)
option(ENABLE_DRIVER_MIC_985XX "Enable mic-985xx serial temperature/humidity meter driver" ON)
option(ENABLE_DRIVER_TONDAJ_SL_814 "Enable tondaj-sl-814 serial sound level meter driver" ON)
option(ENABLE_DRIVER_PCE_322A "Enable pce-322a serial sound level meter driver" ON)
option(ENABLE_DRIVER_CENTER_3XX "Enable center-3xx serial thermometer driver" ON)
option(ENABLE_DRIVER_TELEINFO "Enable teleinfo serial French power meter driver" ON)
option(ENABLE_DRIVER_KERN_SCALE "Enable kern-scale serial scale driver" ON)
option(ENABLE_DRIVER_CONRAD_DIGI_35_CPU "Enable conrad-digi-35-cpu driver" ON)
option(ENABLE_DRIVER_HP_59306A "Enable hp-59306a driver" ON)
option(ENABLE_DRIVER_COLEAD_SLM "Enable colead-slm driver" ON)
option(ENABLE_DRIVER_ICSTATION_USBRELAY "Enable icstation-usbrelay driver" ON)
option(ENABLE_DRIVER_ZKETECH_EBD_USB "Enable zketech-ebd-usb driver" ON)
option(ENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO "Enable arachnid-labs-re-load-pro driver" ON)
option(ENABLE_DRIVER_ASIX_OMEGA_RTM_CLI "Enable asix-omega-rtm-cli driver" ON)
option(ENABLE_DRIVER_KECHENG_KC_330B "Enable kecheng-kc-330b driver" ON)
option(ENABLE_DRIVER_HP_3457A "Enable hp-3457a driver" ON)
option(ENABLE_DRIVER_MICROCHIP_PICKIT2 "Enable microchip-pickit2 driver" ON)
option(ENABLE_DRIVER_HP_3478A "Enable hp-3478a driver" ON)
option(ENABLE_DRIVER_CEM_DT_885X "Enable cem-dt-885x driver" ON)
option(ENABLE_DRIVER_ATORCH "Enable atorch driver" ON)
option(ENABLE_DRIVER_BKPRECISION_1856D "Enable bkprecision-1856d driver" ON)
option(ENABLE_DRIVER_GWINSTEK_GPD "Enable gwinstek-gpd driver" ON)
option(ENABLE_DRIVER_SCPI_DMM "Enable scpi-dmm driver" ON)
option(ENABLE_DRIVER_SERIAL_LCR "Enable serial-lcr driver" ON)
option(ENABLE_DRIVER_JUNTEK_JDS6600 "Enable juntek-jds6600 driver" ON)
option(ENABLE_DRIVER_GMC_MH_1X_2X "Enable gmc-mh-1x-2x driver" ON)
option(ENABLE_DRIVER_GREATFET "Enable greatfet driver" ON)
option(ENABLE_DRIVER_DCTTECH_USBRELAY "Enable dcttech-usbrelay driver" ON)
option(ENABLE_DRIVER_DEVANTECH_ETH008 "Enable devantech-eth008 driver" ON)

#===============================================================================
#= decoder_test option
#-------------------------------------------------------------------------------
option(BUILD_DECODER_TEST "Build the C decoder test program" OFF)
