#ifndef PXVIEW_CORE_DOCUMENTREGISTRY_H
#define PXVIEW_CORE_DOCUMENTREGISTRY_H

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../data/sessiondocument.h"

namespace pv {

class SigSession;

namespace core {

class EventBus;

/**
 * DocumentRegistry — owns the SessionDocument list (all_documents /
 * active_document) and the capture-owner lifecycle (CaptureOwnerGuard +
 * copy thread + capture_state_mutex). Extracted from SigSession (SubTask 10.2)
 * as a mechanical refactoring: no behavior change.
 *
 * The registry holds an injected EventBus* (for broadcast_msg /
 * trigger_message) and a SigSession* (for accessing _is_working / is_working()
 * and other session state). Declared as a friend of SigSession so it can touch
 * private members.
 *
 * ---------------------- modernize-core-layer-final Task 6 ----------------------
 * OWNERSHIP SEMANTICS (final evaluation, no code change):
 *
 * `_active_document` / `_capture_owner_document` / `_all_documents` are
 * NON-OWNING pointers. The actual owners are external to DocumentRegistry:
 *   * TabContext (View layer, see tabcontext.cpp:52 `delete _document`)
 *     owns the per-tab SessionDocument and drives its full lifecycle.
 *   * SessionService (API layer, see session_service.cpp:307/322
 *     `delete _api_document`) owns MCP-created API documents.
 *
 * DocumentRegistry only registers/unregisters these externally-owned
 * documents into its tracking list and (for the capture owner) guards the
 * cross-layer hand-off via CaptureOwnerGuard RAII. Converting to
 * shared_ptr/weak_ptr would require touching TabContext + SessionService —
 * both OUTSIDE the Core layer and outside this spec's scope — and would
 * force the View/API layers to also adopt shared_ptr semantics, breaking
 * the current "TabContext owns document, SigSession borrows it" contract
 * that .pxc save/restore and tab close rely on.
 *
 * Conclusion: risk > reward. The pointers stay raw, with the ownership
 * contract documented here. CaptureOwnerGuard already eliminates the
 * historical UAF on _capture_owner_document. The only mutation paths are
 * register_document/unregister_document/set_active_document/
 * acquire_capture_owner/release_capture_owner — all go through public
 * methods, no external code mutates the raw pointers directly.
 * --------------------------------------------------------------------------- */
class DocumentRegistry {
public:
  // RAII guard for _capture_owner_document + _is_working lifecycle.
  // Constructed in start_capture on success; destructed in stop_capture /
  // clear_capture_owner_document (tab close). Manages owner field + _is_working
  // flag + join_copy_thread + CaptureOwnerChanged broadcast as a single unit,
  // eliminating manual clear_capture_owner_document() calls and use-after-free
  // risk on the background copy thread.
  //
  // NOTE: copy_to_doc_done does NOT reset this guard — in repeat mode the owner
  // field is cleared per-frame but _is_working must stay true across frames.
  // The guard persists for the whole capture session.
  class CaptureOwnerGuard {
  public:
    CaptureOwnerGuard(DocumentRegistry *reg, data::SessionDocument *doc);
    ~CaptureOwnerGuard();
    // Disable copy
    CaptureOwnerGuard(const CaptureOwnerGuard &) = delete;
    CaptureOwnerGuard &operator=(const CaptureOwnerGuard &) = delete;
    // Allow move
    CaptureOwnerGuard(CaptureOwnerGuard &&o) noexcept;
    CaptureOwnerGuard &operator=(CaptureOwnerGuard &&o) noexcept;
    inline data::SessionDocument *doc() const { return _doc; }

  private:
    DocumentRegistry *_registry;
    data::SessionDocument *_doc;
  };

public:
  DocumentRegistry(EventBus *bus, SigSession *session);
  ~DocumentRegistry();

  // --- Document list management ---
  void register_document(data::SessionDocument *doc);
  void unregister_document(data::SessionDocument *doc);
  void set_active_document(data::SessionDocument *doc);
  inline data::SessionDocument *get_active_document() const {
    return _active_document;
  }
  inline const std::vector<data::SessionDocument *> &get_all_documents() const {
    return _all_documents;
  }
  void clear_all_documents_decoders();

  // --- Capture owner / copy thread ---
  inline data::SessionDocument *get_capture_owner_document() const {
    return _capture_owner_document;
  }
  inline bool is_copy_in_progress() const { return _copy_in_progress; }
  void clear_capture_owner_document(data::SessionDocument *doc);
  void join_copy_thread();

  // Called by start_capture to acquire the capture owner guard.
  void acquire_capture_owner(data::SessionDocument *doc);

  // Called by stop_capture / action_stop_capture to release the guard.
  void release_capture_owner();

  // Mutex + copy_thread accessors for the copy_to_doc_done flow in SigSession
  // (which needs to atomically snapshot _copy_in_progress + owner, and launch
  // the copy thread). SigSession is a friend of DocumentRegistry.
  inline std::mutex &capture_state_mutex() { return _capture_state_mutex; }
  inline std::atomic<bool> &copy_in_progress() { return _copy_in_progress; }
  inline std::thread &copy_thread() { return _copy_thread; }
  inline data::SessionDocument *&capture_owner_document() {
    return _capture_owner_document;
  }

private:
  EventBus *_event_bus;
  // Circular reference: CaptureOwnerGuard (nested class) writes the
  // SigSession::_is_working flag and reads SigSession::is_working(). That
  // flag is also read/written directly by CaptureManager and DataFeedParser,
  // so moving _is_working into DocumentRegistry would require coordinated
  // updates across all three managers plus SigSession — too risky for this
  // task. This is a known tech debt tracked by modernize-core-layer-final
  // Task 7.
  SigSession *_session;

  // Document list
  data::SessionDocument *_active_document;
  std::vector<data::SessionDocument *> _all_documents;

  // Capture owner / copy thread state
  mutable std::mutex _capture_state_mutex;
  std::atomic<bool> _copy_in_progress;
  data::SessionDocument *_capture_owner_document;
  std::unique_ptr<CaptureOwnerGuard> _capture_owner_guard;
  std::thread _copy_thread;

  // CaptureOwnerGuard is a nested class of DocumentRegistry, so under C++11+
  // rules it has implicit access to private members without an explicit
  // friend declaration. SigSession uses only public accessors.
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_DOCUMENTREGISTRY_H
