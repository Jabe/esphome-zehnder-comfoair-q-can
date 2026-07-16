# esphome-zehnder-comfoair-q-can

ESPHome component for Zehnder ComfoAir Q ventilation units (Q350/Q450/Q600) via CAN bus.

All PDO decoding, value scaling, text mappings and commands live inside the component; entities are
configured declaratively through `sensor`/`binary_sensor`/`text_sensor`/`button` platforms. The
component automatically requests exactly the PDOs needed for the configured entities.

```yaml
zehnder_comfoair_q:
  id: comfoair

sensor:
  - platform: zehnder_comfoair_q
    supply_air_temp:
      name: Supply Air Temp
    heat_recovery_ratio:
      name: Heat Recovery Ratio

text_sensor:
  - platform: zehnder_comfoair_q
    operating_mode:
      name: Operating Mode

button:
  - platform: zehnder_comfoair_q
    boost_15min:
      name: Boost 15 Min.
```

See [zehnder-comfoair-q.example.yml](zehnder-comfoair-q.example.yml) for a full example
configuration (ESP32 with LAN8720 ethernet and the internal CAN controller of the ESP32). All available entity keys are listed in
[sensor.py](components/zehnder_comfoair_q/sensor.py),
[binary_sensor.py](components/zehnder_comfoair_q/binary_sensor.py),
[text_sensor.py](components/zehnder_comfoair_q/text_sensor.py) and
[button.py](components/zehnder_comfoair_q/button.py) — including pre-heater and GHE (ground heat
exchanger) sensors for units that have them installed.

## Origin

Based on the `zehnder_comfoair_q` component from
[felixstorm/esphome-custom-components](https://github.com/felixstorm/esphome-custom-components)
(commit `42a4179`), which in turn is heavily based on:

- https://github.com/vekexasia/comfoair-esp32
- https://github.com/marco-hoyer/zcan
- https://github.com/michaelarnauts/comfoconnect (good protocol documentation)

## License

GPL-3.0, see [LICENSE](LICENSE) (inherited from the original repository).
