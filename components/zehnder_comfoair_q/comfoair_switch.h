#pragma once

// this header lands in esphome.h even when no switch entity is configured
// (and the switch component sources are then absent from the build)
#include "esphome/core/defines.h"
#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

#include "zehnder_comfoair_q.h"

namespace esphome::zehnder_comfoair_q {

// Manual mode (permanent). State is synced from PDO 49 (operating mode == 5).
class ComfoAirQManualModeSwitch final : public switch_::Switch, public Parented<ZehnderComfoAirQ> {
 protected:
  void write_state(bool state) override { this->parent_->set_manual_mode(state); }
};

}  // namespace esphome::zehnder_comfoair_q

#endif  // USE_SWITCH
