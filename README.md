# esphome-zehnder-comfoair-q-can

ESPHome component for Zehnder ComfoAir Q ventilation units (Q350/Q450/Q600) via CAN bus.

Work in progress: migrating the `zehnder_comfoair_q` component to a modern, standalone ESPHome component design.

## Origin

The component was taken 1:1 from
[felixstorm/esphome-custom-components](https://github.com/felixstorm/esphome-custom-components)
(commit `42a4179`, "Update zehnder_comfoair_q for ESPHome 2025.11"), which in turn is heavily based on:

- https://github.com/vekexasia/comfoair-esp32
- https://github.com/marco-hoyer/zcan
- https://github.com/michaelarnauts/comfoconnect (good protocol documentation)

## Usage

See [zehnder_comfoair_q_example.yaml](zehnder_comfoair_q_example.yaml) for a full example configuration.

## License

GPL-3.0, see [LICENSE](LICENSE) (inherited from the original repository).
