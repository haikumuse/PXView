#include "documentregistry.h"

#include "eventbus.h"
#include "../sigsession.h"
#include "../data/decoderstack.h"
#include "../dsvdef.h"
#include "../log.h"

#include <algorithm>

namespace pv {
namespace core {

// ---------------------------------------------------------------------------
// CaptureOwnerGuard
// ---------------------------------------------------------------------------

DocumentRegistry::CaptureOwnerGuard::CaptureOwnerGuard(DocumentRegistry *reg,
                                                       data::SessionDocument *doc)
    : _registry(reg), _doc(doc) {
  std::lock_guard<std::mutex> lock(_registry->_capture_state_mutex);
  _registry->_capture_owner_document = _doc;
  _registry->_session->_is_working = true;
  _registry->_event_bus->broadcast_msg(
      DSV_MSG_CAPTURE_OWNER_CHANGED,
      _registry->_session->is_working() ? 1 : 0);
}

DocumentRegistry::CaptureOwnerGuard::~CaptureOwnerGuard() {
  if (_registry) {
    // join_copy_thread() MUST be outside the lock — the copy thread may
    // need to acquire _capture_state_mutex (e.g. in copy_to_doc_done).
    _registry->join_copy_thread();
    {
      std::lock_guard<std::mutex> lock(_registry->_capture_state_mutex);
      _registry->_capture_owner_document = nullptr;
      _registry->_session->_is_working = false;
    }
    // Broadcast outside the lock to minimize critical section and avoid
    // listener callbacks re-entering the mutex.
    _registry->_event_bus->broadcast_msg(DSV_MSG_CAPTURE_OWNER_CHANGED, 0);
  }
}

DocumentRegistry::CaptureOwnerGuard::CaptureOwnerGuard(CaptureOwnerGuard &&o) noexcept
    : _registry(o._registry), _doc(o._doc) {
  o._registry = nullptr;
}

DocumentRegistry::CaptureOwnerGuard &
DocumentRegistry::CaptureOwnerGuard::operator=(CaptureOwnerGuard &&o) noexcept {
  if (this != &o) {
    if (_registry) {
      // join_copy_thread() MUST be outside the lock — see destructor note.
      _registry->join_copy_thread();
      {
        std::lock_guard<std::mutex> lock(_registry->_capture_state_mutex);
        _registry->_capture_owner_document = nullptr;
        _registry->_session->_is_working = false;
      }
      _registry->_event_bus->broadcast_msg(DSV_MSG_CAPTURE_OWNER_CHANGED, 0);
    }
    _registry = o._registry;
    _doc = o._doc;
    o._registry = nullptr;
  }
  return *this;
}

// ---------------------------------------------------------------------------
// DocumentRegistry
// ---------------------------------------------------------------------------

DocumentRegistry::DocumentRegistry(EventBus *bus, SigSession *session)
    : _event_bus(bus), _session(session), _active_document(nullptr),
      _copy_in_progress(false), _capture_owner_document(nullptr) {}

DocumentRegistry::~DocumentRegistry() {
  // Join any in-flight copy thread before destruction (a joinable std::thread
  // would otherwise std::terminate on destruction).
  join_copy_thread();
}

void DocumentRegistry::register_document(data::SessionDocument *doc) {
  _all_documents.push_back(doc);
}

void DocumentRegistry::unregister_document(data::SessionDocument *doc) {
  auto it = std::find(_all_documents.begin(), _all_documents.end(), doc);
  if (it != _all_documents.end())
    _all_documents.erase(it);
}

void DocumentRegistry::set_active_document(data::SessionDocument *doc) {
  if (_active_document == doc) // 去重，避免重复广播
    return;
  _active_document = doc;
  // R1: notify listeners that the active document changed. trigger_message
  // also broadcasts via broadcast_msg so Core/headless listeners are reached.
  _event_bus->trigger_message(DSV_MSG_ACTIVE_DOCUMENT_CHANGED);
}

void DocumentRegistry::clear_all_documents_decoders() {
  for (auto doc : _all_documents) {
    auto &stacks = doc->get_decoder_stacks();
    for (auto stack : stacks) {
      if (stack->IsRunning()) {
        stack->_delete_flag = true;
      }
    }
    stacks.clear();
  }
}

void DocumentRegistry::clear_capture_owner_document(data::SessionDocument *doc) {
  // Task 4: Guard-managed — reset the guard when the caller asks to clear the
  // document that is currently the capture owner. Guard destructor handles
  // join_copy_thread() + owner clear + _is_working=false + broadcast.
  // C4 fix: lock the mutex to get a consistent snapshot of
  // _capture_owner_guard and _capture_owner_document. The guard.reset() call
  // happens OUTSIDE the lock — the guard destructor calls join_copy_thread()
  // which could block, and we must not hold the mutex during that (the copy
  // thread may need to acquire _capture_state_mutex in copy_to_doc_done).
  std::unique_ptr<CaptureOwnerGuard> guard_to_reset;
  {
    std::lock_guard<std::mutex> lock(_capture_state_mutex);
    if (_capture_owner_guard && _capture_owner_document == doc) {
      guard_to_reset = std::move(_capture_owner_guard);
    }
  }
  // Reset outside the lock — guard destructor calls join_copy_thread()
  // which could block, and we don't want to hold the mutex during that.
  guard_to_reset.reset();
}

void DocumentRegistry::join_copy_thread() {
  if (_copy_thread.joinable()) {
    _copy_thread.join();
  }
}

void DocumentRegistry::acquire_capture_owner(data::SessionDocument *doc) {
  _capture_owner_guard = std::make_unique<CaptureOwnerGuard>(this, doc);
}

void DocumentRegistry::release_capture_owner() {
  _capture_owner_guard.reset();
}

} // namespace core
} // namespace pv
