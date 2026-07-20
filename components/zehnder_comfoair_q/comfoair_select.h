#pragma once

// this header lands in esphome.h even when no select entity is configured
// (and the select component sources are then absent from the build)
#include "esphome/core/defines.h"
#ifdef USE_SELECT

#include "esphome/components/select/select.h"
#include "esphome/core/helpers.h"

#include "zehnder_comfoair_q.h"

namespace esphome::zehnder_comfoair_q {

enum class SelectPurpose : uint8_t {
  // synced from a PDO; option index = PDO value
  FAN_LEVEL,            // PDO 65, set_level
  BYPASS_MODE,          // PDO 66, set_bypass_mode
  TEMPERATURE_PROFILE,  // PDO 67, set_temp_profile
  // RMI properties without a state PDO; state is read back from the unit
  PASSIVE_TEMPERATURE,  // OffAutoOn
  HUMIDITY_COMFORT,     // OffAutoOn
  HUMIDITY_PROTECTION,  // OffAutoOn
};

class ComfoAirQSelect final : public select::Select, public Parented<ZehnderComfoAirQ> {
 public:
  void set_purpose(SelectPurpose purpose) { this->purpose_ = purpose; }

 protected:
  void control(const std::string &value) override;

  SelectPurpose purpose_{SelectPurpose::FAN_LEVEL};
};

}  // namespace esphome::zehnder_comfoair_q

#endif  // USE_SELECT
