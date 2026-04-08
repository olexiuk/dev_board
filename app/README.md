# Wireless Tilt Controller and Simple Ball Physics Simulation
A simple tilt controller that reads raw accelerometer data from a MPU-6050 via I2C and sends a bluetooth notification to control a ball physics simulation on running on the central device.

## Build Commands
### Peripheral Build
``` bash
west build -p always -b nrf52840dk/nrf52840 app -- -DCONF_FILE="prj.conf;conf/peripheral.conf" -DDTC_OVERLAY_FILE=boards/peripheral_board.overlay
```
### Central Build
```bash
west build -p always -b nrf52840dk/nrf52840 --shield=adafruit_2_8_tft_touch_v2 app -- -DCONF_FILE="prj.conf;conf/central.conf" -DDTC_OVERLAY_FILE=boards/central_board.overlay
```
