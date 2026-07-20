#pragma once

// this header lands in esphome.h even when no button entity is configured
// (and the button component sources are then absent from the build)
#include "esphome/core/defines.h"
#ifdef USE_BUTTON

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "zehnder_comfoair_q.h"

namespace esphome::zehnder_comfoair_q {

// Runs the unit's fan level timer ("Party Timer" on the display): fan level
// for an explicit duration. Duration 0 cancels a running timer.
class ComfoAirQFanLevelTimerButton final : public button::Button, public Parented<ZehnderComfoAirQ> {
 public:
  void set_timer(uint8_t level, uint32_t duration_secs) {
    this->level_ = level;
    this->duration_secs_ = duration_secs;
  }

 protected:
  void press_action() override { this->parent_->set_level_timer(this->level_, this->duration_secs_); }

  uint8_t level_{3};
  uint32_t duration_secs_{0};
};

}  // namespace esphome::zehnder_comfoair_q

#endif  // USE_BUTTON
