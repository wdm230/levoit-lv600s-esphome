# Levoit LV600S ESPHome

ESPHome replacement firmware for the Levoit LV600S / LUH-A602S-WUS humidifier.

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

The ESP32-SOLO-1 is single-core, so the YAML enables `CONFIG_FREERTOS_UNICORE`.

## Features

Working controls:

- Power
- Mode
- Display on/off
- Target humidity
- Mist level 1-9
- Warm mist level 0-3
- Timer minutes
- Query status
- Clear timer

Working status:

- Current humidity
- Current temperature
- Water low/empty status
- Tank removed status
- Humidifying status
- Timer remaining
- Raw diagnostic fields disabled by default, including ACK/result frames and
  reserved status bytes

Raw diagnostic commands are included for manual mode, humidity mode, and sleep
mode. Stock firmware reports an `auto` mode, but no separate direct MCU auto
command was found in the decompiled work-mode path.


## Install

This is an ESPHome external component. The usual way to use it is to reference
this repo from your own ESPHome YAML with `external_components`.

Create a new ESPHome device YAML, or copy `lv600s.yaml` from this repo, then use:

```yaml
external_components:
  - source: github://wdm230/levoit-lv600s-esphome@main
    components: [lv600s_humidifier]
```

If you cloned this repo locally and are building from inside the repo folder, the
included `lv600s.yaml` uses the local component path instead:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [lv600s_humidifier]
```

Copy the example secrets file:

```text
secrets.yaml.example -> secrets.yaml
```

Edit `secrets.yaml` with your Wi-Fi credentials.

Build and upload:

```powershell
uvx esphome upload lv600s.yaml
```

For OTA after the device is online:

```powershell
uvx esphome upload lv600s.yaml --device <device-ip>
```

## Important Notes

- `logger.baud_rate` must stay `0`; GPIO16/GPIO17 are the appliance UART.
- Keep a stock firmware dump before flashing if you want a recovery path.

## Protocol

See [docs/protocol.md](docs/protocol.md) for the UART frame format, command IDs, and
status payload offsets. See [docs/firmware-decompile.md](docs/firmware-decompile.md)
for the stock firmware decompile summary.
