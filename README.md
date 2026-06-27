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
- Water/tank status
- Raw diagnostic fields disabled by default

Raw diagnostic commands are included for manual mode, humidity mode, and sleep/auto
mode. Their exact user-facing mapping still needs more testing.

The `Mode` select is optimistic because the MCU's raw status mode byte does not
currently map cleanly back to the three user-facing mode labels.

## Install

Copy or clone this repository into your ESPHome config directory, then copy:

```text
secrets.yaml.example -> secrets.yaml
```

Edit `secrets.yaml` with your Wi-Fi credentials.

Build/upload:

```powershell
uvx esphome upload lv600s.yaml
```

For OTA after the device is online:

```powershell
uvx esphome upload lv600s.yaml --device <device-ip>
```

## Important Notes

- `logger.baud_rate` must stay `0`; GPIO16/GPIO17 are the appliance UART.
- Do not connect external 5 V serial logic to the appliance UART.
- Keep a stock firmware dump before flashing if you want a recovery path.
- This repo does not include stock firmware, extracted binaries, or logic-analyzer captures.

## Recovery

If you made your own stock flash dump, restore it with a command like:

```powershell
python -m esptool --chip esp32 --port <serial-port> --baud 460800 write-flash 0x0 <path-to-stock-dump.bin>
```

You may need to hold GPIO0 low while resetting/powering the ESP32 to enter the ROM
bootloader.

## Protocol

See [docs/protocol.md](docs/protocol.md) for the UART frame format, command IDs, and
status payload offsets.
