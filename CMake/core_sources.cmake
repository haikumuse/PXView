#===============================================================================
#= PXView sources — split into Core (no Qt Widgets) and GUI (View) layers
#-------------------------------------------------------------------------------

# pxview-core: Core layer (no Qt Widgets dependency).
# Contains: SigSession, DeviceAgent, SignalModel/LissajousModel, DecoderStack/
# SpectrumStack/MathStack, DataSource, SessionDocument/SessionSnapshot,
# SessionService/RpcDispatcher/Transports, utility, config.
# AppControl stays in GUI layer because it references QWidget for top-window tracking.
set(PXVIEW_CORE_SOURCES
    # Core session/orchestration
    PXView/pv/log.cpp
    PXView/pv/core/eventbus.cpp
    PXView/pv/core/sessionstatecontext.cpp
    PXView/pv/core/filterprocessor.cpp
    PXView/pv/core/decodetaskmanager.cpp
    PXView/pv/core/datafeedparser.cpp
    PXView/pv/core/documentregistry.cpp
    PXView/pv/core/capturemanager.cpp
    PXView/pv/sigsession.cpp
    PXView/pv/sessionmanager.cpp
    PXView/pv/deviceagent.cpp
    PXView/pv/dstimer.cpp
    PXView/pv/eventobject.cpp
    PXView/pv/dsvdef.cpp
    PXView/pv/ZipMaker.cpp
    PXView/pv/storesession.cpp
    PXView/pv/tabcontext.cpp
    # NOTE: data/*.cpp live in pv/data/CMakeLists.txt (pxview-data STATIC lib)
    # API/remote-control layer (SessionService, transports, RPC dispatcher)
    PXView/pv/api/session_service.cpp
    PXView/pv/api/app_service.cpp
    PXView/pv/api/rpc_dispatcher.cpp
    PXView/pv/api/ws_transport.cpp
    PXView/pv/api/mcp_transport.cpp
    # NOTE: utility/*.cpp live in PXView/pv/utility/CMakeLists.txt (pxview-utility STATIC lib)
    # NOTE: config/*.cpp live in PXView/pv/config/CMakeLists.txt (pxview-config STATIC lib)
)
