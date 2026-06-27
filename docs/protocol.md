# Levoit LV600S ESPHome Protocol Notes

These notes document the working ESPHome replacement firmware path for the Levoit
LV600S / LUH-A602S-WUS humidifier. The stock module is an ESP32-SOLO-1
(`ESP32-S0WD`, single-core) connected to the appliance MCU over a 3.3 V UART.

## Status

- Stock firmware was dumped and can be restored.
- Stock ESP32 UART configuration was confirmed from the firmware binary.
- ESPHome firmware using the stock `A5` protocol works on the original ESP32 module.
- The older logic-analyzer `55` stream was useful during discovery, but is not the
  final replacement-firmware protocol.

## Hardware

Stock ESP32 appliance UART:

| Setting | Value |
| --- | --- |
| ESP32 chip | ESP32-SOLO-1 / ESP32-S0WD |
| UART peripheral | UART1 |
| TX to appliance MCU | GPIO17 |
| RX from appliance MCU | GPIO16 |
| Baud | 9600 |
| Framing | 8N1, idle high, 3.3 V logic |

ESPHome must disable serial logging so it does not collide with the appliance UART:

```yaml
logger:
  baud_rate: 0
```

The ESP32-SOLO-1 is single-core, so ESPHome must be built with:

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
    sdkconfig_options:
      CONFIG_FREERTOS_UNICORE: y
```

## Stock Firmware Evidence

The stock app image was dumped from the original ESP32 module, converted to ELF,
and analyzed in Ghidra. Those firmware files are intentionally not included in
this repository.

Useful local/private artifacts during reverse engineering:

```text
stock flash dump
extracted OTA app image
converted ELF
Ghidra UART/config analysis output
Ghidra caller/constant analysis output
```

Firmware lookup values:

| Firmware config flag | Value | Meaning |
| --- | --- | --- |
| `0x13` | `0x11` | TX GPIO17 |
| `0x14` | `0x10` | RX GPIO16 |
| `0x15` | `0x2580` | 9600 baud |

Call chain:

```text
platform init
  -> humidifier init
  -> device_info_get_int(0x14) = 16
  -> device_info_get_int(0x13) = 17
  -> device_info_get_int(0x15) = 9600
  -> vesync_uart_tl_init(rx=16, tx=17, baud=9600)
  -> vhal_uart_init(rx=16, tx=17, uart_num=1, baud=9600)
```

## A5 Frame Format

The appliance protocol frame is:

```text
A5 CTRL SEQ LEN_L LEN_H CHECKSUM BODY...
```

Fields:

| Field | Meaning |
| --- | --- |
| `A5` | Frame header |
| `CTRL` | `0x22` for command with ACK request; ACK frames use a different control value |
| `SEQ` | Incrementing sequence byte |
| `LEN_L LEN_H` | Little-endian body length |
| `CHECKSUM` | Ones-complement checksum |
| `BODY` | Command body |

Checksum:

```text
CHECKSUM = (~sum(frame bytes with checksum byte set to 0)) & 0xFF
```

Command body:

```text
01 CMD_L CMD_H 00 PAYLOAD...
```

The command ID is little-endian.

## Commands

Confirmed from stock firmware:

| Function | Command | Payload | Notes |
| --- | --- | --- | --- |
| Query status | `0x4110` | empty | Returns status payload |
| Power | `0xA000` | `[bool]` | `00` off, `01` on |
| Display | `0xA105` | `[level]` | `00` off, `64` on |
| Target humidity | `0xA2E8` | `[00, target]` | Target range 40-80 |
| Timer | `0xA264` | `uint32_le seconds` | Max `43200` seconds |
| Mist level | `0x4113` | `[level, 00, 00, 00]` | App supports 1-9 |
| Warm level | `0x4112` | `[level, 00, 00, 00, 00]` | Valid 0-3 |
| Manual mode level | `0xA260` | `[warm_enable, mist_enable, level]` | Raw command exposed as diagnostic |
| Humidity mode | `0x4114` | `[value, 00, 00, 00]` | Raw command exposed as diagnostic |
| Sleep/auto mode | `0x4082` | `[value, 00, 00, 00, 00, 00]` | Raw command exposed as diagnostic |
| MCU reboot | `0xD101` | `[value]` | Production/maintenance; do not expose normally |
| UART test | `0xD007` | empty | Production/maintenance; do not expose normally |

ACK/status-related IDs observed in the stock handler:

| Command | Notes |
| --- | --- |
| `0xA003` | ACK/result bookkeeping |
| `0xA265` | Timer ACK path |
| `0xA2E8` | Target humidity ACK path |
| `0xA260` | Manual-mode ACK path |

## Status Payload

The status response to command `0x4110` contains a 20-byte payload after the
4-byte command body header.

Fields decoded from firmware log strings:

| Offset | Meaning |
| --- | --- |
| `0` | MCU/protocol version major |
| `1` | MCU/protocol version minor |
| `2` | MCU/protocol version patch |
| `3` | Power/enabled |
| `4` | Container/tank state |
| `5` | Water lacks |
| `6` | Display config |
| `7` | Fog status |
| `8` | Target humidity |
| `9` | Current humidity |
| `10` | Current temperature |
| `11` | Mode |
| `12` | Mist level |
| `13` | Warm level |
| `14` | Other exception |

ESPHome currently publishes the user-facing humidity and temperature from offsets
`9` and `10`.

## ESPHome Component

Current working component:

```text
components/lv600s_humidifier/
  __init__.py
  lv600s_humidifier.h
  lv600s_humidifier.cpp
```

Current working YAML:

```text
lv600s.yaml
```

Primary exposed controls:

- Power switch
- Display switch
- Target humidity number
- Mist level number, 1-9
- Warm level number, 0-3
- Timer minutes number
- Query status button
- Clear timer button

Primary exposed sensors:

- Current humidity
- Current temperature
- Water lacks

Diagnostic/disabled-by-default entities:

- Raw status fields
- MCU version
- Last A5 frame
- Manual mode raw controls
- Humidity mode raw control
- Sleep/auto raw control

## Flashing

Build and upload ESPHome:

```powershell
uvx esphome upload lv600s.yaml
```

For OTA after the device is online:

```powershell
uvx esphome upload lv600s.yaml --device <device-ip>
```

Restore stock firmware if needed:

```powershell
python -m esptool --chip esp32 --port <serial-port> --baud 460800 write-flash 0x0 <path-to-stock-dump.bin>
```

## Open Questions

- Exact user-facing mapping for mode values.
- Exact behavior of humidity mode and sleep/auto mode raw values.
- Whether timer/display behavior is identical across all LV600S hardware revisions.
- Whether this same component works unchanged on related Levoit/VeSync models.
