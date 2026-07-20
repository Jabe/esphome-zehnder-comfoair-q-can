#include "comfoair_select.h"

#ifdef USE_SELECT

namespace esphome::zehnder_comfoair_q {

void ComfoAirQSelect::control(const std::string &value) {
  const auto idx = this->index_of(value);
  if (!idx.has_value())
    return;

  switch (this->purpose_) {
    // PDO-synced selects: the new state is published when the unit pushes the PDO update
    case SelectPurpose::FAN_LEVEL:
      this->parent_->set_level(*idx);
      break;
    case SelectPurpose::BYPASS_MODE:
      this->parent_->set_bypass_mode(static_cast<BypassMode>(*idx));
      break;
    case SelectPurpose::TEMPERATURE_PROFILE:
      this->parent_->set_temp_profile(static_cast<TemperatureProfile>(*idx));
      break;
    // RMI selects have no state PDO: read the property back after setting it —
    // the set and the reads run through the same command queue, in order
    case SelectPurpose::PASSIVE_TEMPERATURE:
      this->parent_->set_temperature_passive(static_cast<OffAutoOn>(*idx));
      this->parent_->refresh_properties();
      break;
    case SelectPurpose::HUMIDITY_COMFORT:
      this->parent_->set_humidity_comfort(static_cast<OffAutoOn>(*idx));
      this->parent_->refresh_properties();
      break;
    case SelectPurpose::HUMIDITY_PROTECTION:
      this->parent_->set_humidity_protection(static_cast<OffAutoOn>(*idx));
      this->parent_->refresh_properties();
      break;
  }
}

}  // namespace esphome::zehnder_comfoair_q

#endif  // USE_SELECT
