#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "zehnder_comfoair_q.h"

namespace esphome::zehnder_comfoair_q {

enum class ButtonAction : uint8_t {
  BOOST,                // value = duration in seconds, 0 = off
  FAN_LEVEL,            // value = level 0..3
  MANUAL_MODE,          // value = 0/1
  PASSIVE_TEMPERATURE,  // value = OffAutoOn
  HUMIDITY_COMFORT,     // value = OffAutoOn
};

class ComfoAirQButton final : public button::Button, public Parented<ZehnderComfoAirQ> {
 public:
  void set_action(ButtonAction action, uint32_t value) {
    this->action_ = action;
    this->value_ = value;
  }

 protected:
  void press_action() override;

  ButtonAction action_{ButtonAction::BOOST};
  uint32_t value_{0};
};

}  // namespace esphome::zehnder_comfoair_q
