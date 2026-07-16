# esphome-zehnder-comfoair-q-can

ESPHome component for Zehnder ComfoAir Q ventilation units (Q350/Q450/Q600) via CAN bus.

All PDO decoding, value scaling, text mappings and commands live inside the component; entities are
configured declaratively through `sensor`/`binary_sensor`/`text_sensor`/`button` platforms. The
component automatically requests exactly the PDOs needed for the configured entities.

## Usage

The repo is organized as small, combinable package modules — a node config is just a
shopping list of packages:

```yaml
substitutions:
  device_name: zehnder-comfoair-q-can

api:
  encryption:
    key: !secret api_encryption_key

packages:
  base: github://Jabe/esphome-zehnder-comfoair-q-can/packages/core/base.yml@v1.0.0
  board: github://Jabe/esphome-zehnder-comfoair-q-can/packages/boards/esp32dev.yml@v1.0.0
  network: github://Jabe/esphome-zehnder-comfoair-q-can/packages/connectivity/ethernet-lan8720.yml@v1.0.0
  comfoair: github://Jabe/esphome-zehnder-comfoair-q-can/packages/comfoair/base.yml@v1.0.0
  sensors: github://Jabe/esphome-zehnder-comfoair-q-can/packages/comfoair/sensors.yml@v1.0.0
  controls: github://Jabe/esphome-zehnder-comfoair-q-can/packages/comfoair/controls.yml@v1.0.0
```

Available packages:

- `core/base.yml` — esphome identity, logger, api, ota, component source
- `boards/esp32dev.yml` — generic ESP32 board (esp-idf), e.g. Olimex ESP32-PoE
- `connectivity/ethernet-lan8720.yml` — LAN8720 ethernet (Olimex ESP32-PoE pin defaults)
- `comfoair/base.yml` — CAN bus + component hub (CAN pins via substitutions)
- `comfoair/sensors.yml` — all standard sensor/binary_sensor/text_sensor entities
- `comfoair/computed.yml` — computed sensors (temp diffs, heat/enthalpy recovery ratio)
- `comfoair/controls.yml` — control buttons (boost, fan level, manual mode, ...)
- `comfoair/pre_heater.yml` — pre-heater entities (optional hardware)
- `comfoair/ghe.yml` — ComfoFond/ground-heat-exchanger entities (optional hardware)

Defaults are set via `substitutions:` in each package and can be overridden from the node
config (e.g. `entity_prefix`, CAN/ethernet pins). Top-level blocks deep-merge on top of the
packages, so tweaks like esp-idf `advanced:` options need no fork.

See [zehnder-comfoair-q.example.yml](zehnder-comfoair-q.example.yml) for the full remote
shopping list and [zehnder-comfoair-q.local.yml](zehnder-comfoair-q.local.yml) for building
from a git clone. Every entity is optional — the component only requests the CAN PDOs needed
for what you configure. All available entity keys are listed in
[sensor.py](components/zehnder_comfoair_q/sensor.py),
[binary_sensor.py](components/zehnder_comfoair_q/binary_sensor.py),
[text_sensor.py](components/zehnder_comfoair_q/text_sensor.py) and
[button.py](components/zehnder_comfoair_q/button.py) — including pre-heater and GHE (ground
heat exchanger) sensors for units that have them installed.

## Origin

Based on the `zehnder_comfoair_q` component from
[felixstorm/esphome-custom-components](https://github.com/felixstorm/esphome-custom-components)
(commit `42a4179`), which in turn is heavily based on:

- https://github.com/vekexasia/comfoair-esp32
- https://github.com/marco-hoyer/zcan
- https://github.com/michaelarnauts/comfoconnect (good protocol documentation)

## License

GPL-3.0, see [LICENSE](LICENSE) (inherited from the original repository).
