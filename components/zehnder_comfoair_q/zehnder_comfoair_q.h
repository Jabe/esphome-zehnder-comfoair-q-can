#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/canbus/canbus.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <map>
#include <string>
#include <vector>

namespace esphome::zehnder_comfoair_q {

enum TemperatureProfile : uint8_t {
  TEMP_PROFIL_NORMAL = 0,
  TEMP_PROFIL_COOL = 1,
  TEMP_PROFIL_WARM = 2,
};

enum BypassMode : uint8_t {
  BYPASS_AUTO = 0,
  BYPASS_ACTIVATE = 1,
  BYPASS_DEACTIVATE = 2,
};

enum OffAutoOn : uint8_t {
  OAO_OFF = 0,
  OAO_AUTO = 1,
  OAO_ON = 2,
};

// Sensors derived from other PDO values instead of a PDO of their own,
// recalculated on a fixed 60s interval.
enum class ComputedSensor : uint8_t {
  INDOOR_AIR_TEMP_DIFF = 0,
  OUTDOOR_AIR_TEMP_DIFF,
  HEAT_RECOVERY_RATIO,
  ENTHALPY_RECOVERY_RATIO,
  COUNT_,
};

class ZehnderComfoAirQ : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_request_delay(uint32_t request_delay) { this->request_delay_ = request_delay; }
  void set_local_node_id(uint8_t local_node_id) { this->local_node_id_ = local_node_id; }
  // Adds a PDO id to the request cycle (sorted, duplicates ignored). Called for every
  // PDO a registered entity depends on, plus any manually configured request_ids.
  void add_request_id(uint16_t pdo_id);

#ifdef USE_SENSOR
  void register_sensor(uint16_t pdo_id, sensor::Sensor *sens);
  void register_computed_sensor(ComputedSensor kind, sensor::Sensor *sens);
#endif
#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(uint16_t pdo_id, binary_sensor::BinarySensor *bsens);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(uint16_t pdo_id, text_sensor::TextSensor *tsens);
#endif

  void request_all_pdos();
  void request_pdo(uint16_t pdo_id);

  void set_level_float(float state);
  void set_level(uint8_t level);

  void set_boost(uint32_t duration_secs) { send_command_set_timer(duration_secs > 0, 0x01, 0x06, 3, duration_secs); }
  void set_manual_mode(bool enable) { send_command_set_timer(enable, 0x08, 0x01, 1); }
  void set_temp_profile(TemperatureProfile temp_profile) {
    send_command_set_timer(true, 0x02, 0x01, temp_profile, 0xffffffff);
  }
  void set_bypass_mode(BypassMode bypass_mode, uint32_t duration_secs) {
    send_command_set_timer(bypass_mode != BYPASS_AUTO, 0x02, 0x01, bypass_mode);
  }
  void set_temperature_passive(OffAutoOn oao) { send_command_set_property(0x1d /* TEMPHUMCONTROL */, 0x01, 0x04, oao); }
  void set_humidity_comfort(OffAutoOn oao) { send_command_set_property(0x1d /* TEMPHUMCONTROL */, 0x01, 0x06, oao); }
  void set_humidity_protection(OffAutoOn oao) { send_command_set_property(0x1d /* TEMPHUMCONTROL */, 0x01, 0x06, oao); }

  void send_command_set_timer(bool enable, uint8_t subunit_id, uint8_t property_id, uint8_t property_value = 0x00,
                              uint32_t duration_secs = 1 /* constant for timers with pre-defined durations */);
  void send_command_set_property(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id, uint8_t property_value);
  void send_command(const std::vector<uint8_t> &command);

  static std::string seconds_to_human_readable(int seconds);

 protected:
  // Decoding metadata per known PDO: signedness and scale applied to the raw value.
  struct PdoMeta {
    uint16_t pdo_id;
    bool is_unsigned;
    float scale;
  };
  static const PdoMeta *find_pdo_meta_(uint16_t pdo_id);

  struct PdoBinding {
#ifdef USE_SENSOR
    sensor::Sensor *sensor{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
    binary_sensor::BinarySensor *binary_sensor{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
    text_sensor::TextSensor *text_sensor{nullptr};
#endif
  };

  void on_can_frame_(uint32_t can_id, bool extended_id, bool remote_transmission_request,
                     const std::vector<uint8_t> &data);
  void handle_pdo_value_(uint16_t pdo_id, float value);
  void update_computed_sensors_();
  float get_last_pdo_value_(uint16_t pdo_id) const;

  static std::string operating_mode_to_string_(int value);
  static std::string bypass_activation_mode_to_string_(int value);
  static std::string temperature_profile_to_string_(int value);

  canbus::Canbus *canbus_{nullptr};

  std::vector<uint16_t> request_ids_{};
  uint32_t request_delay_;
  size_t request_next_pdo_pos_{0};
  void request_next_pdo_();

  uint8_t local_node_id_{0x3e};

  std::map<uint16_t, PdoBinding> bindings_{};
  std::map<uint16_t, float> last_pdo_values_{};
#ifdef USE_SENSOR
  sensor::Sensor *computed_sensors_[static_cast<size_t>(ComputedSensor::COUNT_)]{};
#endif

  uint32_t get_command_next_can_id_(uint8_t src_node_id, uint8_t dst_node_id, uint8_t unknown_counter,
                                    bool is_multi_message_command, bool response_error_occurred, bool is_request);
  uint32_t get_command_can_id_(uint8_t src_node_id, uint8_t dst_node_id, uint8_t unknown_counter,
                               bool is_multi_message_command, bool response_error_occurred, bool is_request,
                               uint8_t sequence_number);
  uint8_t get_command_next_sequence_number_();
  uint8_t command_sequence_number_{0};

  void send_can_message_(uint32_t can_id, bool remote_transmission_request, const std::vector<uint8_t> &data = {});
};

}  // namespace esphome::zehnder_comfoair_q
