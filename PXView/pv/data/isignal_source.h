#ifndef PXVIEW_PV_DATA_ISIGNAL_SOURCE_H
#define PXVIEW_PV_DATA_ISIGNAL_SOURCE_H

#include <cstdint>
#include <memory>
#include <vector>

namespace pv {
namespace data {

class SignalModel;
class TriggerConfig;

// ISignalSource — signal models + trigger config.
// Spec v2 Task 8: extracted from DataSource胖接口.
class ISignalSource {
public:
    virtual ~ISignalSource() = default;
    virtual std::vector<std::shared_ptr<SignalModel>>& get_signal_models() = 0;
    virtual const TriggerConfig& trigger_config() const = 0;
    virtual uint8_t trigd_ch() = 0;
    virtual bool trigd() = 0;
};

} // namespace data
} // namespace pv

#endif // PXVIEW_PV_DATA_ISIGNAL_SOURCE_H
