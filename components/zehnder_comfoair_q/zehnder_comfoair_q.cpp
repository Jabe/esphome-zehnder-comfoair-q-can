#include "zehnder_comfoair_q.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>

namespace esphome::zehnder_comfoair_q {

static const char *const TAG = "comfoairq";
// separate tag so CAN frame dumps can be silenced via logger: independently
static const char *const TAG_DUMP = "comfoair_dump";

// PDO frames: bits 24-28 clear (no command prefix), bit 6 set, node id in bits 0-5, PDO id in bits 14-23
static constexpr uint32_t PDO_CAN_ID_MASK = 0b11111000000000011111111000000;
static constexpr uint32_t PDO_CAN_ID_MATCH = 0b00000000000000000000001000000;

// PDO ids used by the computed sensors
static constexpr uint16_t PDO_BYPASS_STATE = 227;
static constexpr uint16_t PDO_EXHAUST_FAN_FLOW = 119;
static constexpr uint16_t PDO_SUPPLY_FAN_FLOW = 120;
static constexpr uint16_t PDO_POWER_CONSUMPTION = 128;
static constexpr uint16_t PDO_EXTRACT_AIR_TEMP = 274;
static constexpr uint16_t PDO_EXHAUST_AIR_TEMP = 275;
static constexpr uint16_t PDO_OUTDOOR_AIR_TEMP = 276;
static constexpr uint16_t PDO_SUPPLY_AIR_TEMP = 278;
static constexpr uint16_t PDO_EXTRACT_AIR_HUMIDITY = 290;
static constexpr uint16_t PDO_OUTDOOR_AIR_HUMIDITY = 292;
static constexpr uint16_t PDO_SUPPLY_AIR_HUMIDITY = 294;

// water content of air in g/kg (Magnus formula, standard pressure)
static float water_content_g_kg(float rh, float temp) {
  const float pw_hpa = rh / 100.0f * 6.112f * expf((17.67f * temp) / (temp + 243.5f));
  return 622.0f * pw_hpa / (1013.25f - pw_hpa);
}

// specific enthalpy of moist air in kJ/kg
static float enthalpy_kj_kg(float rh, float temp) {
  const float x_kg_kg = water_content_g_kg(rh, temp) / 1000.0f;
  return 1.006f * temp + x_kg_kg * (2501.0f + 1.86f * temp);
}

// air density at standard pressure in kg/m³
static float air_density_kg_m3(float temp) { return 101325.0f / (287.05f * (temp + 273.15f)); }

// mass flow of an air stream in kg/s (density at the stream's temperature)
static float mass_flow_kg_s(float flow_m3_h, float temp) {
  if (!(flow_m3_h > 0.0f))
    return NAN;
  return flow_m3_h / 3600.0f * air_density_kg_m3(temp);
}

// sensible thermal power of an air stream in W
static float thermal_power_w(float flow_m3_h, float temp_for_density, float delta_t) {
  return mass_flow_kg_s(flow_m3_h, temp_for_density) * 1005.0f * delta_t;
}

// Supply-side recovery ratio in %, NAN when the extract/outdoor difference is
// too small for a meaningful result. Deliberately not clamped — fan waste heat
// can push it slightly above 100%.
static float recovery_ratio(float supply, float outdoor, float extract, float min_denominator) {
  const float denominator = extract - outdoor;
  if (std::abs(denominator) < min_denominator)
    return NAN;
  const float ratio = 100.0f * (supply - outdoor) / denominator;
  // plausibility window: values far outside 0..100% are measurement artifacts
  // (sensor placement, transients), not physics
  if (ratio < -10.0f || ratio > 125.0f)
    return NAN;
  return ratio;
}

const ZehnderComfoAirQ::PdoMeta *ZehnderComfoAirQ::find_pdo_meta_(uint16_t pdo_id) {
  // {pdo_id, is_unsigned, scale}, sorted by pdo_id
  static constexpr PdoMeta PDO_METAS[] = {
      {16, true, 1},      // away indicator (raw, 7 = away)
      {49, true, 1},      // operating mode
      {65, true, 1},      // fan level
      {66, true, 1},      // bypass activation mode
      {67, true, 1},      // temperature profile
      {81, false, 1},     // next fan change in (s, -1 = n/a)
      {82, false, 1},     // next bypass change in (s, -1 = n/a)
      {117, true, 1},     // exhaust fan duty (%)
      {118, true, 1},     // supply fan duty (%)
      {119, false, 1},    // exhaust fan flow (m³/h)
      {120, false, 1},    // supply fan flow (m³/h)
      {121, true, 1},     // exhaust fan speed (rpm)
      {122, true, 1},     // supply fan speed (rpm)
      {128, true, 1},     // power consumption current (W)
      {129, true, 1},     // power consumption ytd (kWh)
      {130, true, 1},     // power consumption since start (kWh)
      {144, true, 1},     // pre heater power consumption current (W)
      {145, true, 1},     // pre heater power consumption ytd (kWh)
      {146, true, 1},     // pre heater power consumption since start (kWh)
      {192, false, 1},    // filter replacement remaining (days)
      {209, false, 0.1},  // running mean outdoor temp (°C)
      {212, false, 0.1},  // profile target temp (°C)
      {213, true, 1e-3},  // avoided heating actual (W -> kW)
      {214, true, 1},     // avoided heating ytd (kWh)
      {215, true, 1},     // avoided heating total (kWh)
      {216, true, 1e-3},  // avoided cooling actual (W -> kW)
      {217, true, 1},     // avoided cooling ytd (kWh)
      {218, true, 1},     // avoided cooling total (kWh)
      {220, false, 0.1},  // pre heater temp before (°C)
      {221, false, 0.1},  // post heater temp after (°C)
      {227, true, 1},     // bypass state (%)
      {274, false, 0.1},  // extract air temp (°C)
      {275, false, 0.1},  // exhaust air temp (°C)
      {276, false, 0.1},  // outdoor air temp (°C)
      {277, false, 0.1},  // pre heater temp after (°C)
      {278, false, 0.1},  // supply air temp (°C)
      {290, true, 1},     // extract air humidity (%)
      {291, true, 1},     // exhaust air humidity (%)
      {292, true, 1},     // outdoor air humidity (%)
      {293, true, 1},     // pre heater humidity after (%)
      {294, true, 1},     // supply air humidity (%)
      {416, false, 0.1},  // ghe outdoor temp (°C)
      {417, false, 0.1},  // ghe sole temp (°C)
      {418, true, 1},     // ghe state (%)
  };

  for (const auto &meta : PDO_METAS) {
    if (meta.pdo_id == pdo_id)
      return &meta;
  }
  return nullptr;
}

void ZehnderComfoAirQ::setup() {
  this->canbus_->add_callback(
      [this](uint32_t can_id, bool extended_id, bool remote_transmission_request, const std::vector<uint8_t> &data) {
        this->on_can_frame_(can_id, extended_id, remote_transmission_request, data);
      });

#ifdef USE_SENSOR
  bool any_computed = false;
  for (auto *sens : this->computed_sensors_)
    any_computed |= sens != nullptr;
  if (any_computed) {
    this->set_interval(60 * 1000, [this]() { this->update_computed_sensors_(); });
  }
#endif

  if (this->has_property_entities_() && this->property_poll_interval_ > 0) {
    this->set_interval(this->property_poll_interval_, [this]() { this->refresh_properties(); });
  }

  // do a first request of all PDOs some time after starting (helpful for long intervals)
  this->set_timeout(10 * 1000, [this]() { this->update(); });
}

void ZehnderComfoAirQ::update() {
  if (!this->request_ids_.empty())
    this->request_all_pdos();
  this->refresh_properties();
}

void ZehnderComfoAirQ::dump_config() {
  ESP_LOGCONFIG(TAG, "Zehnder ComfoAir Q:");
  ESP_LOGCONFIG(TAG, "  Local node id: 0x%02X", this->local_node_id_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Request delay: %" PRIu32 " ms", this->request_delay_);
  std::string ids;
  for (auto pdo_id : this->request_ids_) {
    if (!ids.empty())
      ids += ", ";
    ids += to_string(pdo_id);
  }
  ESP_LOGCONFIG(TAG, "  Requested PDO ids (%d): %s", (int) this->request_ids_.size(), ids.c_str());
}

void ZehnderComfoAirQ::add_request_id(uint16_t pdo_id) {
  auto it = std::lower_bound(this->request_ids_.begin(), this->request_ids_.end(), pdo_id);
  if (it == this->request_ids_.end() || *it != pdo_id)
    this->request_ids_.insert(it, pdo_id);
}

#ifdef USE_SENSOR
void ZehnderComfoAirQ::register_sensor(uint16_t pdo_id, sensor::Sensor *sens) {
  this->bindings_[pdo_id].sensor = sens;
  this->add_request_id(pdo_id);
}

void ZehnderComfoAirQ::register_computed_sensor(ComputedSensor kind, sensor::Sensor *sens) {
  this->computed_sensors_[static_cast<size_t>(kind)] = sens;
  switch (kind) {
    case ComputedSensor::INDOOR_AIR_TEMP_DIFF:
      this->add_request_id(PDO_EXTRACT_AIR_TEMP);
      this->add_request_id(PDO_SUPPLY_AIR_TEMP);
      break;
    case ComputedSensor::OUTDOOR_AIR_TEMP_DIFF:
      this->add_request_id(PDO_EXHAUST_AIR_TEMP);
      this->add_request_id(PDO_OUTDOOR_AIR_TEMP);
      break;
    case ComputedSensor::HEAT_RECOVERY_RATIO:
      this->add_request_id(PDO_BYPASS_STATE);
      this->add_request_id(PDO_EXTRACT_AIR_TEMP);
      this->add_request_id(PDO_OUTDOOR_AIR_TEMP);
      this->add_request_id(PDO_SUPPLY_AIR_TEMP);
      break;
    case ComputedSensor::ENTHALPY_RECOVERY_RATIO:
    case ComputedSensor::HUMIDITY_RECOVERY_RATIO:
      this->add_request_id(PDO_BYPASS_STATE);
      this->add_request_id(PDO_EXTRACT_AIR_TEMP);
      this->add_request_id(PDO_OUTDOOR_AIR_TEMP);
      this->add_request_id(PDO_SUPPLY_AIR_TEMP);
      this->add_request_id(PDO_EXTRACT_AIR_HUMIDITY);
      this->add_request_id(PDO_OUTDOOR_AIR_HUMIDITY);
      this->add_request_id(PDO_SUPPLY_AIR_HUMIDITY);
      break;
    case ComputedSensor::SUPPLY_THERMAL_POWER:
      this->add_request_id(PDO_SUPPLY_FAN_FLOW);
      this->add_request_id(PDO_EXTRACT_AIR_TEMP);
      this->add_request_id(PDO_SUPPLY_AIR_TEMP);
      break;
    case ComputedSensor::VENTILATION_HEAT_LOSS:
      this->add_request_id(PDO_EXHAUST_FAN_FLOW);
      this->add_request_id(PDO_EXHAUST_AIR_TEMP);
      this->add_request_id(PDO_OUTDOOR_AIR_TEMP);
      break;
    case ComputedSensor::LATENT_RECOVERY_POWER:
      this->add_request_id(PDO_SUPPLY_FAN_FLOW);
      this->add_request_id(PDO_OUTDOOR_AIR_TEMP);
      this->add_request_id(PDO_SUPPLY_AIR_TEMP);
      this->add_request_id(PDO_OUTDOOR_AIR_HUMIDITY);
      this->add_request_id(PDO_SUPPLY_AIR_HUMIDITY);
      break;
    case ComputedSensor::SPECIFIC_FAN_POWER:
      this->add_request_id(PDO_POWER_CONSUMPTION);
      this->add_request_id(PDO_EXHAUST_FAN_FLOW);
      this->add_request_id(PDO_SUPPLY_FAN_FLOW);
      break;
    default:
      break;
  }
}
#endif

#ifdef USE_BINARY_SENSOR
void ZehnderComfoAirQ::register_binary_sensor(uint16_t pdo_id, binary_sensor::BinarySensor *bsens) {
  this->bindings_[pdo_id].binary_sensor = bsens;
  this->add_request_id(pdo_id);
}
#endif

#ifdef USE_TEXT_SENSOR
void ZehnderComfoAirQ::register_text_sensor(uint16_t pdo_id, text_sensor::TextSensor *tsens) {
  this->bindings_[pdo_id].text_sensor = tsens;
  this->add_request_id(pdo_id);
}
#endif

#ifdef USE_SELECT
void ZehnderComfoAirQ::register_select(uint16_t pdo_id, select::Select *sel) {
  this->bindings_[pdo_id].select = sel;
  this->add_request_id(pdo_id);
}

void ZehnderComfoAirQ::register_property_select(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id,
                                                select::Select *sel) {
  this->property_selects_.push_back({unit_id, subunit_id, property_id, sel});
}
#endif

#ifdef USE_NUMBER
void ZehnderComfoAirQ::register_property_number(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id,
                                                number::Number *num, float scale) {
  this->property_numbers_.push_back({unit_id, subunit_id, property_id, num, scale});
}
#endif

bool ZehnderComfoAirQ::has_property_entities_() const {
  bool any = false;
#ifdef USE_SELECT
  any |= !this->property_selects_.empty();
#endif
#ifdef USE_NUMBER
  any |= !this->property_numbers_.empty();
#endif
  return any;
}

void ZehnderComfoAirQ::refresh_properties() {
#ifdef USE_SELECT
  for (const auto &binding : this->property_selects_) {
    auto *sel = binding.select;
    this->read_property(binding.unit_id, binding.subunit_id, binding.property_id,
                        [sel](bool ok, const std::vector<uint8_t> &data) {
                          if (!ok) {
                            ESP_LOGW(TAG, "Property read for '%s' failed", sel->get_name().c_str());
                            return;
                          }
                          if (data.size() != 1 || data[0] >= sel->traits.get_options().size()) {
                            ESP_LOGW(TAG, "Unexpected property value for '%s': %s (please report)",
                                     sel->get_name().c_str(), format_hex_pretty(data).c_str());
                            return;
                          }
                          sel->publish_state((size_t) data[0]);
                        });
  }
#endif
#ifdef USE_NUMBER
  for (const auto &binding : this->property_numbers_) {
    auto *num = binding.number;
    const float scale = binding.scale;
    this->read_property(binding.unit_id, binding.subunit_id, binding.property_id,
                        [num, scale](bool ok, const std::vector<uint8_t> &data) {
                          if (!ok) {
                            ESP_LOGW(TAG, "Property read for '%s' failed", num->get_name().c_str());
                            return;
                          }
                          if (data.size() != 2) {
                            ESP_LOGW(TAG, "Unexpected property value for '%s': %s (please report)",
                                     num->get_name().c_str(), format_hex_pretty(data).c_str());
                            return;
                          }
                          const auto raw = (int16_t) ((uint16_t) data[0] | ((uint16_t) data[1] << 8));
                          num->publish_state(raw * scale);
                        });
  }
#endif
}

#ifdef USE_SWITCH
void ZehnderComfoAirQ::register_switch(uint16_t pdo_id, switch_::Switch *sw) {
  this->bindings_[pdo_id].switch_ = sw;
  this->add_request_id(pdo_id);
}
#endif

void ZehnderComfoAirQ::on_can_frame_(uint32_t can_id, bool extended_id, bool remote_transmission_request,
                                     const std::vector<uint8_t> &data) {
  const bool is_pdo = extended_id && (can_id & PDO_CAN_ID_MASK) == PDO_CAN_ID_MATCH;
  if (!is_pdo) {
    if (extended_id && (can_id >> 24) == 0x1f && !remote_transmission_request) {
      this->handle_command_frame_(can_id, data);
    } else {
      ESP_LOGV(TAG_DUMP, "can_id: 0x%08" PRIx32 ", rtr: %d, length: %d, content: %s", can_id,
               remote_transmission_request, (int) data.size(),
               remote_transmission_request ? "n/a" : format_hex_pretty(data).c_str());
    }
    return;
  }

  ESP_LOGD(TAG_DUMP, "Node: %d, PDO: %d, rtr: %d, length: %d, content: %s (can_id: 0x%08" PRIx32 ")",
           (int) (can_id & 0x3f), (int) (can_id >> 14), remote_transmission_request, (int) data.size(),
           remote_transmission_request ? "n/a" : format_hex_pretty(data).c_str(), can_id);
  if (remote_transmission_request)
    return;

  const uint16_t pdo_id = can_id >> 14;
  const auto *meta = find_pdo_meta_(pdo_id);
  if (meta == nullptr)
    return;

  float value;
  switch (data.size()) {
    case 1:
      value = data[0];
      break;
    case 2: {
      const uint16_t raw = (uint16_t) data[0] | ((uint16_t) data[1] << 8);
      value = meta->is_unsigned ? (float) raw : (float) (int16_t) raw;
      break;
    }
    case 4: {
      const uint32_t raw =
          (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
      value = meta->is_unsigned ? (float) raw : (float) (int32_t) raw;
      break;
    }
    default:
      ESP_LOGW(TAG, "Unable to infer type from can message size: %d (PDO: %d)", (int) data.size(), pdo_id);
      return;
  }

  this->handle_pdo_value_(pdo_id, value * meta->scale);
}

void ZehnderComfoAirQ::handle_pdo_value_(uint16_t pdo_id, float value) {
  this->last_pdo_values_[pdo_id] = value;

  // while an emulated level timer runs, its countdown owns the "next fan change" entities
  if (pdo_id == 81 && this->level_timer_active_)
    return;

  // The countdown PDOs tick every second while a timer runs, which would spam
  // HA's recorder with useless state changes. Only publish on jumps of >= 30s
  // (timer set, extended or cancelled) or on transitions from/to "n/a".
  if (pdo_id == 81 || pdo_id == 82) {
    auto last = this->last_published_countdowns_.find(pdo_id);
    if (last != this->last_published_countdowns_.end()) {
      const bool na_transition = (value < 0) != (last->second < 0);
      if (!na_transition && std::abs(value - last->second) < 30.0f)
        return;
    }
    this->last_published_countdowns_[pdo_id] = value;
  }

  auto it = this->bindings_.find(pdo_id);
  if (it == this->bindings_.end())
    return;
  const auto &binding = it->second;

#ifdef USE_SENSOR
  if (binding.sensor != nullptr)
    binding.sensor->publish_state(value);
#endif

#ifdef USE_BINARY_SENSOR
  if (binding.binary_sensor != nullptr) {
    switch (pdo_id) {
      case 16:  // away indicator: value 7 = away
        binding.binary_sensor->publish_state(value == 7.0f);
        break;
      default:
        binding.binary_sensor->publish_state(value != 0.0f);
        break;
    }
  }
#endif

#ifdef USE_SELECT
  if (binding.select != nullptr) {
    // option order matches the PDO value range (0-based)
    const auto idx = (int) value;
    if (idx >= 0 && (size_t) idx < binding.select->traits.get_options().size()) {
      binding.select->publish_state((size_t) idx);
    } else {
      ESP_LOGW(TAG, "PDO %d value %d has no select option", pdo_id, idx);
    }
  }
#endif

#ifdef USE_SWITCH
  if (binding.switch_ != nullptr) {
    switch (pdo_id) {
      case 49: {
        // operating mode: 1 = manual (limited, e.g. fan level set on the display),
        // 5 = manual (permanent) — both mean the unit is not following its schedule
        const int mode = (int) value;
        binding.switch_->publish_state(mode == 1 || mode == 5);
        break;
      }
      default:
        binding.switch_->publish_state(value != 0.0f);
        break;
    }
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (binding.text_sensor != nullptr) {
    switch (pdo_id) {
      case 49:
        binding.text_sensor->publish_state(operating_mode_to_string_((int) value));
        break;
      case 66:
        binding.text_sensor->publish_state(bypass_activation_mode_to_string_((int) value));
        break;
      case 67:
        binding.text_sensor->publish_state(temperature_profile_to_string_((int) value));
        break;
      case 81:
      case 82:
        binding.text_sensor->publish_state(seconds_to_human_readable((int) value));
        break;
      default:
        binding.text_sensor->publish_state(to_string((int) value));
        break;
    }
  }
#endif
}

float ZehnderComfoAirQ::get_last_pdo_value_(uint16_t pdo_id) const {
  auto it = this->last_pdo_values_.find(pdo_id);
  return it != this->last_pdo_values_.end() ? it->second : NAN;
}

#ifdef USE_SENSOR
void ZehnderComfoAirQ::update_computed_sensors_() {
  const float extract_temp = this->get_last_pdo_value_(PDO_EXTRACT_AIR_TEMP);
  const float exhaust_temp = this->get_last_pdo_value_(PDO_EXHAUST_AIR_TEMP);
  const float outdoor_temp = this->get_last_pdo_value_(PDO_OUTDOOR_AIR_TEMP);
  const float supply_temp = this->get_last_pdo_value_(PDO_SUPPLY_AIR_TEMP);

  auto *indoor_diff = this->computed_sensors_[static_cast<size_t>(ComputedSensor::INDOOR_AIR_TEMP_DIFF)];
  if (indoor_diff != nullptr)
    indoor_diff->publish_state(supply_temp - extract_temp);

  auto *outdoor_diff = this->computed_sensors_[static_cast<size_t>(ComputedSensor::OUTDOOR_AIR_TEMP_DIFF)];
  if (outdoor_diff != nullptr)
    outdoor_diff->publish_state(exhaust_temp - outdoor_temp);

  // The recovery ratios describe the exchanger itself, not the weather: they
  // are only (re)measured when the bypass is fully closed and the differences
  // are large enough — otherwise the last measured value simply stays
  // published instead of flipping to unknown for weeks of mild weather.
  const bool exchanger_measurable = this->get_last_pdo_value_(PDO_BYPASS_STATE) == 0.0f;
  const auto publish_ratio = [](sensor::Sensor *sens, float ratio) {
    if (sens != nullptr && !std::isnan(ratio))
      sens->publish_state(ratio);
  };

  auto *heat_recovery = this->computed_sensors_[static_cast<size_t>(ComputedSensor::HEAT_RECOVERY_RATIO)];
  auto *enthalpy_recovery = this->computed_sensors_[static_cast<size_t>(ComputedSensor::ENTHALPY_RECOVERY_RATIO)];
  auto *humidity_recovery = this->computed_sensors_[static_cast<size_t>(ComputedSensor::HUMIDITY_RECOVERY_RATIO)];
  if (exchanger_measurable) {
    publish_ratio(heat_recovery, recovery_ratio(supply_temp, outdoor_temp, extract_temp, 2.0f /* K */));

    if (enthalpy_recovery != nullptr || humidity_recovery != nullptr) {
      const float supply_rh = this->get_last_pdo_value_(PDO_SUPPLY_AIR_HUMIDITY);
      const float outdoor_rh = this->get_last_pdo_value_(PDO_OUTDOOR_AIR_HUMIDITY);
      const float extract_rh = this->get_last_pdo_value_(PDO_EXTRACT_AIR_HUMIDITY);

      publish_ratio(enthalpy_recovery,
                    recovery_ratio(enthalpy_kj_kg(supply_rh, supply_temp), enthalpy_kj_kg(outdoor_rh, outdoor_temp),
                                   enthalpy_kj_kg(extract_rh, extract_temp), 2.0f /* kJ/kg */));
      // 1 g/kg minimum: the unit reports humidity in whole percent (~0.15 g/kg
      // resolution at 20°C), below that the ratio is quantization noise
      publish_ratio(humidity_recovery,
                    recovery_ratio(water_content_g_kg(supply_rh, supply_temp),
                                   water_content_g_kg(outdoor_rh, outdoor_temp),
                                   water_content_g_kg(extract_rh, extract_temp), 1.0f /* g/kg */));
    }
  }

  const float supply_flow = this->get_last_pdo_value_(PDO_SUPPLY_FAN_FLOW);
  const float exhaust_flow = this->get_last_pdo_value_(PDO_EXHAUST_FAN_FLOW);

  auto *supply_power = this->computed_sensors_[static_cast<size_t>(ComputedSensor::SUPPLY_THERMAL_POWER)];
  if (supply_power != nullptr) {
    supply_power->publish_state(thermal_power_w(supply_flow, supply_temp, supply_temp - extract_temp));
  }

  auto *heat_loss = this->computed_sensors_[static_cast<size_t>(ComputedSensor::VENTILATION_HEAT_LOSS)];
  if (heat_loss != nullptr) {
    heat_loss->publish_state(thermal_power_w(exhaust_flow, exhaust_temp, exhaust_temp - outdoor_temp));
  }

  auto *latent_power = this->computed_sensors_[static_cast<size_t>(ComputedSensor::LATENT_RECOVERY_POWER)];
  if (latent_power != nullptr) {
    const float delta_x_kg_kg =
        (water_content_g_kg(this->get_last_pdo_value_(PDO_SUPPLY_AIR_HUMIDITY), supply_temp) -
         water_content_g_kg(this->get_last_pdo_value_(PDO_OUTDOOR_AIR_HUMIDITY), outdoor_temp)) /
        1000.0f;
    // evaporation enthalpy of water: 2501 kJ/kg
    latent_power->publish_state(mass_flow_kg_s(supply_flow, supply_temp) * delta_x_kg_kg * 2.501e6f);
  }

  auto *specific_fan_power = this->computed_sensors_[static_cast<size_t>(ComputedSensor::SPECIFIC_FAN_POWER)];
  if (specific_fan_power != nullptr) {
    // electrical power per average air volume flow (Wh/m³), rises when filters clog
    const float average_flow = (supply_flow + exhaust_flow) / 2.0f;
    const float power = this->get_last_pdo_value_(PDO_POWER_CONSUMPTION);
    specific_fan_power->publish_state(average_flow > 0.0f ? power / average_flow : NAN);
  }
}
#endif

std::string ZehnderComfoAirQ::operating_mode_to_string_(int value) {
  switch (value) {
    case 1:
      return "Manual (limited)";
    case 2:
      return "PRESETRF";
    case 3:
      return "PRESETANALOG";
    case 4:
      return "PRESETRFANALOG";
    case 5:
      return "Manual (permanent)";
    case 6:
      return "Boost";
    case 7:
      return "Boost (RF)";
    case 8:
      return "Bathroom Switch";
    case 11:
      return "Away";
    case 255:
      return "Auto";
    default:
      return to_string(value);
  }
}

std::string ZehnderComfoAirQ::bypass_activation_mode_to_string_(int value) {
  switch (value) {
    case 0:
      return "Auto";
    case 1:
      return "Activated";
    case 2:
      return "Deactivated";
    default:
      return to_string(value);
  }
}

std::string ZehnderComfoAirQ::temperature_profile_to_string_(int value) {
  switch (value) {
    case 0:
      return "Normal";
    case 1:
      return "Cold";
    case 2:
      return "Warm";
    default:
      return to_string(value);
  }
}

std::string ZehnderComfoAirQ::seconds_to_human_readable(int seconds) {
  if (seconds < 0)
    return "n/a";

  std::string output;
  if (output.length() > 0 || seconds >= 86400) {
    int days = seconds / 86400;
    output += to_string(days) + "d";
    seconds -= days * 86400;
  }
  if (output.length() > 0 || seconds >= 3600) {
    int hours = seconds / 3600;
    output += to_string(hours) + "h";
    seconds -= hours * 3600;
  }
  if (output.length() > 0 || seconds >= 60) {
    int minutes = seconds / 60;
    output += to_string(minutes) + "m";
    seconds -= minutes * 60;
  }
  output += to_string(seconds) + "s";

  return output;
}

void ZehnderComfoAirQ::request_all_pdos() {
  ESP_LOGD(TAG, "Requesting all registered PDOs (count: %d)", (int) this->request_ids_.size());

  if (this->request_next_pdo_pos_ == 0) {
    this->request_next_pdo_();
  } else {
    ESP_LOGW(TAG, "Skipping PDO request cycle as last cycle is still in progress.");
  }
}

void ZehnderComfoAirQ::request_next_pdo_() {
  if (this->request_next_pdo_pos_ >= this->request_ids_.size())
    return;

  this->request_pdo(this->request_ids_[this->request_next_pdo_pos_]);
  this->request_next_pdo_pos_++;
  if (this->request_next_pdo_pos_ < this->request_ids_.size()) {
    this->set_timeout(this->request_delay_, [this]() { this->request_next_pdo_(); });
  } else {
    this->request_next_pdo_pos_ = 0;
  }
}

void ZehnderComfoAirQ::request_pdo(uint16_t pdo_id) {
  // the unit does not care for the data length to be set correctly on transmission requests,
  // so there is no need to send any data
  this->send_can_message_((pdo_id << 14) + 0x40 + this->local_node_id_, true);
}

void ZehnderComfoAirQ::set_level_float(float state) {
  uint8_t level;
  if (state >= 1)
    level = 3;
  else if (state >= 0.5)
    level = 2;
  else if (state > 0)
    level = 1;
  else
    level = 0;
  ESP_LOGD(TAG, "send_cmd_level_float: state: %f, level: %d", state, level);

  this->set_level(level);
}

void ZehnderComfoAirQ::set_level(uint8_t level) {
  // a new override replaces an emulated timer including its auto-revert
  this->cancel_emulated_level_timer_();
  this->send_command_set_timer(true, 0x01, 0x01, level);
}

void ZehnderComfoAirQ::set_level_timer(uint8_t level, uint32_t duration_secs) {
  if (duration_secs == 0) {  // cancel both mechanisms
    this->send_command_set_timer(false, 0x01, 0x06);
    if (this->level_timer_active_) {
      this->cancel_emulated_level_timer_();
      this->set_manual_mode(false);
    }
    return;
  }

  if (level == 3) {
    // native boost timer: display countdown, survives ESP reboots
    if (this->level_timer_active_) {
      // clear a running emulated timer including its override, the boost takes over
      this->cancel_emulated_level_timer_();
      this->set_manual_mode(false);
    }
    this->send_command_set_timer(true, 0x01, 0x06, 3, duration_secs);
    return;
  }

  // Emulated timer: 0x01 override plus ESP-side auto-revert. No display
  // countdown; after an ESP reboot the timer is gone and the unit's own
  // default override duration acts as the backstop.
  this->send_command_set_timer(false, 0x01, 0x06);  // a running boost would take priority
  this->set_level(level);
  this->level_timer_active_ = true;
  this->level_timer_end_ms_ = millis() + duration_secs * 1000;
  this->set_timeout("level_timer", duration_secs * 1000, [this]() {
    this->cancel_emulated_level_timer_();
    this->set_manual_mode(false);
  });
  this->set_interval("level_timer_tick", 30 * 1000, [this]() { this->publish_next_fan_change_remaining_(); });
  this->publish_next_fan_change_remaining_();
}

void ZehnderComfoAirQ::cancel_emulated_level_timer_() {
  if (!this->level_timer_active_)
    return;
  this->cancel_timeout("level_timer");
  this->cancel_interval("level_timer_tick");
  this->level_timer_active_ = false;
  this->publish_next_fan_change_(-1);
}

void ZehnderComfoAirQ::publish_next_fan_change_(int seconds) {
  auto it = this->bindings_.find(81);
  if (it == this->bindings_.end())
    return;
#ifdef USE_SENSOR
  if (it->second.sensor != nullptr)
    it->second.sensor->publish_state(seconds);
#endif
#ifdef USE_TEXT_SENSOR
  if (it->second.text_sensor != nullptr)
    it->second.text_sensor->publish_state(seconds_to_human_readable(seconds));
#endif
}

void ZehnderComfoAirQ::publish_next_fan_change_remaining_() {
  const auto remaining_ms = (int32_t) (this->level_timer_end_ms_ - millis());
  this->publish_next_fan_change_(remaining_ms > 0 ? remaining_ms / 1000 : 0);
}

void ZehnderComfoAirQ::send_command_set_timer(bool enable, uint8_t subunit_id, uint8_t property_id,
                                              uint8_t property_value, uint32_t duration_secs) {
  std::vector<uint8_t> command = {(uint8_t) (enable ? 0x84 : 0x85), 0x15 /* SCHEDULE */, subunit_id, property_id};
  if (enable) {
    command.insert(command.end(), {0x00, 0x00, 0x00, 0x00, (uint8_t) (duration_secs), (uint8_t) (duration_secs >> 8),
                                   (uint8_t) (duration_secs >> 16), (uint8_t) (duration_secs >> 24), property_value});
  }
  this->send_command(command);
}

void ZehnderComfoAirQ::send_command_set_property(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id,
                                                 uint8_t property_value) {
  this->send_command({0x03, unit_id, subunit_id, property_id, property_value});
}

void ZehnderComfoAirQ::send_command_set_property16(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id,
                                                   int16_t property_value) {
  this->send_command({0x03, unit_id, subunit_id, property_id, (uint8_t) property_value,
                      (uint8_t) (((uint16_t) property_value) >> 8)});
}

void ZehnderComfoAirQ::send_command(const std::vector<uint8_t> &command, rmi_callback_t callback) {
  this->rmi_queue_.push_back({command, std::move(callback)});
  this->send_next_rmi_();
}

void ZehnderComfoAirQ::read_property(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id,
                                     rmi_callback_t callback) {
  // command 0x01 = read, type 0x10 = actual value
  this->send_command({0x01, unit_id, subunit_id, 0x10, property_id}, std::move(callback));
}

void ZehnderComfoAirQ::send_next_rmi_() {
  if (this->rmi_in_flight_ || this->rmi_queue_.empty())
    return;

  const auto &command = this->rmi_queue_.front().command;
  this->rmi_in_flight_ = true;
  this->rmi_in_flight_seq_ = this->get_command_next_sequence_number_();
  this->rmi_response_next_frame_ = 0;
  this->rmi_response_buffer_.clear();

  const bool is_multi_message_command = command.size() > 8;
  const auto can_id = this->get_command_can_id_(this->local_node_id_, 0x01, 0, is_multi_message_command, false, true,
                                                this->rmi_in_flight_seq_);

  if (is_multi_message_command) {
    std::vector<uint8_t> message_buffer;
    int message_counter = 0;
    for (auto command_pos = command.begin(); command_pos < command.end(); command_pos += 7) {
      const bool is_last_message = command.end() - command_pos <= 7;
      message_buffer.clear();
      message_buffer.push_back(message_counter | (is_last_message ? 0x80 : 0));
      message_buffer.insert(message_buffer.end(), command_pos, std::min(command_pos + 7, command.end()));
      this->send_can_message_(can_id, false, message_buffer);

      message_counter++;
    }
  } else {
    this->send_can_message_(can_id, false, command);
  }

  this->set_timeout("rmi_response", 1500, [this]() {
    ESP_LOGW(TAG, "Timeout waiting for command response (seq %d)", this->rmi_in_flight_seq_);
    this->finish_rmi_(false, {});
  });
}

void ZehnderComfoAirQ::finish_rmi_(bool ok, const std::vector<uint8_t> &data) {
  if (!this->rmi_in_flight_)
    return;
  this->cancel_timeout("rmi_response");
  auto request = std::move(this->rmi_queue_.front());
  this->rmi_queue_.pop_front();
  this->rmi_in_flight_ = false;
  if (request.callback)
    request.callback(ok, data);
  this->send_next_rmi_();
}

void ZehnderComfoAirQ::handle_command_frame_(uint32_t can_id, const std::vector<uint8_t> &data) {
  const uint8_t dst_node_id = (can_id >> 6) & 0x3f;
  const bool is_request = can_id & (1 << 16);
  if (dst_node_id != this->local_node_id_ || is_request) {
    // other bus participants talking to each other (or to us as a request)
    ESP_LOGV(TAG_DUMP, "command frame: can_id: 0x%08" PRIx32 ", length: %d, content: %s", can_id, (int) data.size(),
             format_hex_pretty(data).c_str());
    return;
  }

  ESP_LOGD(TAG, "Command response: can_id: 0x%08" PRIx32 ", seq: %d, error: %d, length: %d, content: %s", can_id,
           (int) ((can_id >> 17) & 0x3), (int) ((can_id >> 15) & 1), (int) data.size(),
           format_hex_pretty(data).c_str());

  if (!this->rmi_in_flight_) {
    ESP_LOGW(TAG, "Unexpected command response (no command in flight)");
    return;
  }
  const uint8_t sequence_number = (can_id >> 17) & 0x3;
  if (sequence_number != this->rmi_in_flight_seq_) {
    ESP_LOGW(TAG, "Command response with unexpected sequence number %d (expected %d)", sequence_number,
             this->rmi_in_flight_seq_);
    return;
  }

  if (can_id & (1 << 15)) {  // error flag
    ESP_LOGW(TAG, "Command failed, error response: %s", format_hex_pretty(data).c_str());
    this->finish_rmi_(false, data);
    return;
  }

  if ((can_id & (1 << 14)) == 0) {  // single frame response
    this->finish_rmi_(true, data);
    return;
  }

  // multi frame response: first byte is the frame counter, bit 7 marks the last frame
  if (data.empty())
    return;
  const uint8_t frame_counter = data[0] & 0x7f;
  const bool is_last_frame = data[0] & 0x80;
  if (frame_counter != this->rmi_response_next_frame_) {
    ESP_LOGW(TAG, "Command response frame out of order (%d, expected %d)", frame_counter,
             this->rmi_response_next_frame_);
    this->finish_rmi_(false, {});
    return;
  }
  this->rmi_response_next_frame_++;
  this->rmi_response_buffer_.insert(this->rmi_response_buffer_.end(), data.begin() + 1, data.end());
  if (is_last_frame)
    this->finish_rmi_(true, this->rmi_response_buffer_);
}

uint32_t ZehnderComfoAirQ::get_command_can_id_(uint8_t src_node_id, uint8_t dst_node_id, uint8_t unknown_counter,
                                               bool is_multi_message_command, bool response_error_occurred,
                                               bool is_request, uint8_t sequence_number) {
  return 0x1f << 24 |                                //
         sequence_number << 17 |                     //
         (is_request ? 1 : 0) << 16 |                //
         (response_error_occurred ? 1 : 0) << 15 |   //
         (is_multi_message_command ? 1 : 0) << 14 |  //
         unknown_counter << 12 |                     //
         dst_node_id << 6 |                          //
         src_node_id << 0;
}

uint8_t ZehnderComfoAirQ::get_command_next_sequence_number_() {
  this->command_sequence_number_ = (this->command_sequence_number_ + 1) & 0x3;
  return this->command_sequence_number_;
}

void ZehnderComfoAirQ::send_can_message_(uint32_t can_id, bool remote_transmission_request,
                                         const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Send can message: id: 0x%08" PRIx32 " (pdo_id: %" PRIu32 "), rtr: %d, size: %d, content: %s", can_id,
           can_id >> 14, remote_transmission_request, (int) data.size(), format_hex_pretty(data).c_str());

  if (this->canbus_ == nullptr) {
    ESP_LOGE(TAG, "Canbus not set, exiting send_can_message_.");
    return;
  }

  this->canbus_->send_data(can_id, true, remote_transmission_request, data);
}

}  // namespace esphome::zehnder_comfoair_q
