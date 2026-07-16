#include "comfoair_button.h"

namespace esphome::zehnder_comfoair_q {

void ComfoAirQButton::press_action() {
  switch (this->action_) {
    case ButtonAction::BOOST:
      this->parent_->set_boost(this->value_);
      break;
    case ButtonAction::FAN_LEVEL:
      this->parent_->set_level(this->value_);
      break;
    case ButtonAction::MANUAL_MODE:
      this->parent_->set_manual_mode(this->value_ != 0);
      break;
    case ButtonAction::PASSIVE_TEMPERATURE:
      this->parent_->set_temperature_passive(static_cast<OffAutoOn>(this->value_));
      break;
    case ButtonAction::HUMIDITY_COMFORT:
      this->parent_->set_humidity_comfort(static_cast<OffAutoOn>(this->value_));
      break;
  }
}

}  // namespace esphome::zehnder_comfoair_q
