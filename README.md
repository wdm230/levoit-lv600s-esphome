# Levoit LV600S ESPHome

ESPHome replacement firmware for the Levoit LV600S humidifier (LUH-A602S-WUS, also
tested on the EU model LUH-A602S-WEUR).

This replaces the stock firmware on the built-in ESP32-SOLO-1 module and talks to
the appliance MCU using the stock `A5` UART protocol.

## Confirmed Hardware

| Item | Value |
| --- | --- |
| ESP chip | ESP32-SOLO-1 / ESP32-S0WD |
| UART | UART1 |
| ESP TX to appliance MCU | GPIO17 |
| ESP RX from appliance MCU | GPIO16 |
| Baud | 9600 8N1 |
| Logic level | 3.3 V |
| MCU firmware seen | 56.0.1 (LUH-A602S-WEUR) |

The ESP32-SOLO-1 is single-core, so the YAML enables `CONFIG_FREERTOS_UNICORE`.

## Features

Controls (native entities, updated as soon as the MCU reports a change):

- Power
- Mode: Auto, Manual, Sleep, Target Humidity (reflects the mode reported by the MCU)
- Display on/off
- Target humidity 40-80 %
- Mist level 1-9
- Warm mist level 0-3
- Timer minutes (shows the duration of the running timer, 0 when none)
- Query status
- Clear timer
- Start setup hotspot

Status:

- Current humidity and temperature
- Water low/empty and tank removed
- Humidifying
- Timer remaining (counted down locally, like the stock firmware)
- Panel button event (`pair_wifi`, `factory_reset`, `action_1`, `action_2`)
- Raw diagnostic fields, disabled by default (status bytes, MCU version, last
  frame/ACK)

Device behaviour:

- **Wi-Fi icon on the panel** is driven like stock: on while Home Assistant is
  connected, slow blink while it is not, fast blink while the setup hotspot is up.
  It is left alone while the panel is dark (power off, display off, sleep mode)
  and re-sent when the display comes back on.
- **Long press → setup hotspot.** The panel long press that the stock firmware uses
  for Wi-Fi pairing (MCU button action 3) opens the `LV600S Setup` hotspot with
  the captive portal, while the device stays connected to your Wi-Fi. Use it to
  move the humidifier to another network. It closes after `hotspot_timeout`, or
  as soon as new credentials entered in the portal connect. The same can be
  triggered with the **Start Setup Hotspot** button.

Selecting **Auto** from Home Assistant is not supported: no MCU command for auto
mode is known (the stock firmware only sets it app-side). Auto selected on the
panel is shown correctly.

## Install

This is an ESPHome external component. Reference it from your own ESPHome YAML,
or copy `lv600s.yaml` from this repo:

```yaml
external_components:
  - source: github://wdm230/levoit-lv600s-esphome@main
    components: [lv600s_humidifier]
```

The included `lv600s.yaml` uses the local component path, for building from inside
a clone of this repo:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [lv600s_humidifier]
```

Copy `secrets.yaml.example` to `secrets.yaml` and fill in your Wi-Fi credentials.

> **Put real Wi-Fi credentials in `secrets.yaml`.** ESPHome stores credentials
> entered in the captive portal under a key derived from the config hash, so they
> are forgotten after any YAML change and the device falls back to the
> `LV600S Setup` hotspot.

Build and upload:

```sh
uvx esphome run lv600s.yaml
```

For OTA after the device is online:

```sh
uvx esphome run lv600s.yaml --device <device-ip> --no-logs
```

### First flash over serial

1. Connect a 3.3 V USB-UART adapter to the module's TX/RX/GND, hold GPIO0 low and
   power-cycle the module.
2. Back up the stock firmware first. It contains the device's VeSync factory data
   (`fac_data` partition) and is the only way back:

   ```sh
   uvx esptool --port /dev/ttyUSB0 read-flash 0 ALL lv600s_stock.bin
   uvx esptool --port /dev/ttyUSB0 verify-flash 0x0 lv600s_stock.bin
   ```

3. Flash `.esphome/build/lv600s-humidifier/build/firmware.factory.bin` at `0x0`
   (erase first so the stock NVS does not overlap ESPHome's partition table):

   ```sh
   uvx esptool --port /dev/ttyUSB0 write-flash --erase-all 0x0 firmware.factory.bin
   ```

4. Release GPIO0 and power-cycle the module.

If your adapter has no RTS/DTR wired to EN/GPIO0, esptool cannot reset the chip:
pass `--before no-reset --after no-reset`, and keep using the same baud rate
between calls (the flasher stub stays running at the last one).

Restore stock firmware:

```sh
uvx esptool --port /dev/ttyUSB0 write-flash 0x0 lv600s_stock.bin
```

## Configuration

```yaml
lv600s_humidifier:
  id: lv600s
  uart_id: mcu_uart
  status_interval: 30s        # safety-net polling; the MCU pushes changes itself
  wifi_led: true              # drive the Wi-Fi icon on the panel
  hotspot_on_long_press: true # open the setup hotspot on the pairing long press
  hotspot_timeout: 15min      # close the hotspot again after this time
  # Sensors/binary sensors: current_humidity, current_temperature, water_lacks,
  # tank_removed, humidifying, timer_remaining, power, display, warm_enabled,
  # target_humidity, mist_level, warm_level, mode, ... (see lv600s.yaml)

switch:
  - platform: lv600s_humidifier
    power: { name: "Power" }
    display: { name: "Display" }

select:
  - platform: lv600s_humidifier
    mode: { name: "Mode" }

number:
  - platform: lv600s_humidifier
    target_humidity: { name: "Target Humidity", mode: slider }
    mist_level: { name: "Mist Level", mode: slider }
    warm_level: { name: "Warm Level", mode: slider }
    timer_minutes: { name: "Timer Minutes", mode: box }

button:
  - platform: lv600s_humidifier
    query_status: { name: "Query Status" }
    clear_timer: { name: "Clear Timer" }
    start_hotspot: { name: "Start Setup Hotspot" }

event:
  - platform: lv600s_humidifier
    panel_button: { name: "Panel Button" }
```

The hotspot feature needs `wifi: ap:` and `captive_portal:` in the YAML.

## Changes in this fork

Found by comparing the component with a disassembly of the stock firmware:

1. **Mode reflects the device.** The select is derived from the MCU's mode byte
   (0 auto, 1 manual, 2 sleep, 3 humidity) instead of being optimistic.
2. **Native entities.** Switches, numbers, select and buttons are component
   platforms that publish when the MCU reports, instead of template entities
   polled every 60 s. Rejected changes snap back immediately.
3. **Timer tracking.** Timer remaining is counted down locally from the
   acknowledged timer command and resynced from MCU timer reports (`0xA266`,
   timers set on the panel). The MCU answers a `0xA265` query with an empty
   payload, so after an ESP reboot the timer is assumed off.
4. **Status pushes.** The MCU sends `0x4111` (same 20-byte layout as `0x4110`)
   when its state changes; these were dropped before. Polling went from 5 s to
   30 s.
5. **Frame types and results.** The CTRL byte is decoded (version nibble 2,
   `0x10` ACK, `0x20` ACK requested, `0x40` error) and the result byte of
   replies is checked, so rejected commands are logged and resynced.
6. **ACKs for MCU requests.** Panel button actions (`0xD102`/`0xD103`) and the
   self-test request (`0xD104`) are acknowledged like stock
   (`A5 12 <seq> 04 00 <sum> 01 CMD_L CMD_H 00`); unknown requests get an error
   ACK. Button action 3 opens the setup hotspot.
7. **Wi-Fi LED.** `0xA129` with `[state, on_ms LE16, off_ms LE16, 0]`
   (0 off, 1 on, 2 blink).
8. **TX queue.** Commands are sent one at a time and wait for the MCU's ACK
   (500 ms timeout, 2 retries), like the stock UART queue. Repeated settings
   (e.g. a dragged slider) are coalesced.

Also: an incomplete RX frame is discarded after 100 ms of silence, and the timer
remaining sensor has the `duration` device class.

## Important Notes

- `logger.baud_rate` must stay `0`; GPIO16/GPIO17 are the appliance UART.
- Keep a stock firmware dump before flashing if you want a recovery path.
- While the setup hotspot is up, the captive portal is also reachable from your
  LAN on port 80.
- Entity names match the original YAML, so Home Assistant entity IDs are kept
  when upgrading.

## Protocol

See [docs/protocol.md](docs/protocol.md) for the UART frame format, command IDs, and
status payload offsets. See [docs/firmware-decompile.md](docs/firmware-decompile.md)
for the stock firmware decompile summary.
