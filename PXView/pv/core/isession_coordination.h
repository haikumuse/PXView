#ifndef PXVIEW_CORE_ISESSION_COORDINATION_H
#define PXVIEW_CORE_ISESSION_COORDINATION_H

#include <cstdint>
#include <memory>

struct srd_decoder;
class DecoderStatus;

namespace pv {

// SessionData is defined in the pv namespace (not pv::data)
class SessionData;

namespace data {
class DecoderStack;
namespace decode { class Decoder; }
} // namespace data

/**
 * ISessionCoordination — cross-manager coordination interface.
 *
 * Spec v2 Task 10: Extracts cross-manager coordination methods from
 * SessionStateContext into a pure interface to break the circular
 * dependency between SessionStateContext and its 5 managers.
 *
 * Managers hold an ISessionCoordination* pointer for cross-manager
 * operations, while still holding a SessionStateContext* for shared
 * state access (signal_models, view_data, device_agent, etc.).
 *
 * This interface contains only methods that involve coordination
 * between multiple managers (e.g., decode tasks require DataFeedParser
 * to work with DecodeTaskManager and DocumentRegistry).
 */
class ISessionCoordination {
public:
    virtual ~ISessionCoordination() = default;

    // --- Decode task coordination ---
    // Used by DecodeTaskManager / DataFeedParser to notify when tasks complete
    virtual void clear_all_decode_task2() = 0;
    virtual void add_decode_task(std::shared_ptr<data::DecoderStack> stack) = 0;
    virtual void attach_data_to_signal(SessionData *data) = 0;

    // --- Trigger coordination ---
    // Used by CaptureManager to sync trigger state with libsigrok
    virtual void sync_trigger_to_libsigrok(bool disable_trigger = false) = 0;

    // --- Glitch filter coordination ---
    // Used by FilterProcessor to notify when filter state should be cleared
    virtual void clear_glitch_filter_state_for_capture() = 0;

    // --- Query methods (needed by multiple managers) ---
    virtual uint16_t get_ch_num(int type) = 0;
    virtual uint64_t cur_samplelimits() = 0;
    virtual uint64_t cur_snap_samplerate() = 0;
    virtual void set_cur_snap_samplerate(uint64_t samplerate) = 0;
    virtual void set_cur_samplelimits(uint64_t samplelimits) = 0;
};

} // namespace pv

#endif // PXVIEW_CORE_ISESSION_COORDINATION_H