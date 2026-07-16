#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "zehnder_comfoair_q.h"

namespace esphome::zehnder_comfoair_q {

// Boost timer button; duration 0 switches boost off.
class ComfoAirQBoostButton final : public button::Button, public Parented<ZehnderComfoAirQ> {
 public:
  void set_duration(uint32_t duration_secs) { this->duration_secs_ = duration_secs; }

 protected:
  void press_action() override { this->parent_->set_boost(this->duration_secs_); }

  uint32_t duration_secs_{0};
};

}  // namespace esphome::zehnder_comfoair_q
