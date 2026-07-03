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
    # Data layer (snapshots, models, stacks)
    PXView/pv/data/snapshot.cpp
    PXView/pv/data/signaldata.cpp
    PXView/pv/data/signalmodel.cpp
    PXView/pv/data/datasource.cpp
    PXView/pv/data/triggerconfig.cpp
    PXView/pv/data/lissajousmodel.cpp
    PXView/pv/data/logicsnapshot.cpp
    PXView/pv/data/pulse_analyzer.cpp
    PXView/pv/data/pulse_analyzer.h
    PXView/pv/data/dsosnapshot.cpp
    PXView/pv/data/analogsnapshot.cpp
    PXView/pv/data/sessionsnapshot.cpp
    PXView/pv/data/sessiondocument.cpp
    PXView/pv/data/signalconfigstore.cpp
    PXView/pv/data/decoderstack.cpp
    PXView/pv/data/mathstack.cpp
    PXView/pv/data/spectrumstack.cpp
    PXView/pv/data/disk_buffer_manager.cpp
    PXView/pv/data/disk_write_thread.cpp
    PXView/pv/data/disk_read_cache.cpp
    PXView/pv/data/mmap_allocator.cpp
    PXView/pv/data/decode/rowdata.cpp
    PXView/pv/data/decode/row.cpp
    PXView/pv/data/decode/decoder.cpp
    PXView/pv/data/decode/annotation.cpp
    PXView/pv/data/decode/annotationrestable.cpp
    PXView/pv/data/decode/decoderstatus.cpp
    # API/remote-control layer (SessionService, transports, RPC dispatcher)
    PXView/pv/api/session_service.cpp
    PXView/pv/api/app_service.cpp
    PXView/pv/api/rpc_dispatcher.cpp
    PXView/pv/api/ws_transport.cpp
    PXView/pv/api/mcp_transport.cpp
    # Utility + config (Core-only, no Qt Widgets)
    PXView/pv/utility/encoding.cpp
    PXView/pv/utility/path.cpp
    PXView/pv/utility/array.cpp
    PXView/pv/config/appconfig.cpp
    PXView/pv/config/shortcutdefs.cpp
)
