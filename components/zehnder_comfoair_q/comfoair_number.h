#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"

#include "zehnder_comfoair_q.h"

#include <cmath>

namespace esphome::zehnder_comfoair_q {

// Number backed by an INT16 RMI property (value = raw * scale). The state is
// read back from the unit instead of being published optimistically, so an
// out-of-range write rejected by the unit simply snaps back.
class ComfoAirQPropertyNumber final : public number::Number, public Parented<ZehnderComfoAirQ> {
 public:
  void set_property(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id, float scale) {
    this->unit_id_ = unit_id;
    this->subunit_id_ = subunit_id;
    this->property_id_ = property_id;
    this->scale_ = scale;
  }

 protected:
  void control(float value) override {
    const auto raw = (int16_t) lroundf(value / this->scale_);
    this->parent_->send_command_set_property16(this->unit_id_, this->subunit_id_, this->property_id_, raw);
    this->parent_->refresh_properties();
  }

  uint8_t unit_id_{0};
  uint8_t subunit_id_{0};
  uint8_t property_id_{0};
  float scale_{1.0f};
};

}  // namespace esphome::zehnder_comfoair_q
