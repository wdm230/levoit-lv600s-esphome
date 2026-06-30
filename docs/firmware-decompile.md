# LV600S Stock Firmware Decompile Report

This report summarizes the appliance-relevant findings from the stock LV600S /
LUH-A602S-WUS ESP32 firmware dump in `C:\Users\Will\Desktop\humid`.

The raw firmware artifacts are intentionally kept outside this repository.

## Decompile Artifacts

Local artifacts generated during this pass:

| Artifact | Path | Notes |
| --- | --- | --- |
| Full Ghidra project | `C:\Users\Will\Desktop\humid\ghidra_projects\lv600s.gpr` | Existing project for `lv600s_ota0.elf` |
| Full decompile script | `C:\Users\Will\Desktop\humid\tools\ghidra_scripts\Lv600sDecompileAll.java` | Dumps every non-external function |
| Full decompile output | `C:\Users\Will\Desktop\humid\flash\ghidra_lv600s_all_decompiled.txt` | 6,867 functions, 0 decompile failures |
| Full decompile log | `C:\Users\Will\Desktop\humid\flash\ghidra_lv600s_all_decompiled_headless.log` | Contains a few Xtensa p-code warnings |
| Focused appliance output | `C:\Users\Will\Desktop\humid\flash\ghidra_lv600s_end_to_end.txt` | 468 appliance/UART/cloud-adjacent functions |
| Focused script | `C:\Users\Will\Desktop\humid\tools\ghidra_scripts\Lv600sEndToEnd.java` | Starts from known UART/status roots and string xrefs |

Ghidra decompiler output is not the original source. Most function names are
synthetic, local variable names are synthetic, and some SDK paths are noisy. The
appliance command, status, and mode paths below are stable because they are backed
by constants, log strings, and cross-references in the dumped firmware.

## High-Value Function Map

| Address | Purpose |
| --- | --- |
| `0x40176be4` | Humidifier init |
| `0x401793fc` | MCU status payload decode |
| `0x401796b0` | Timer remaining ACK/report handler |
| `0x401796f4` | Command ACK/result handler |
| `0x401797bc` | Timer report handler |
| `0x4017998c` | Power command builder |
| `0x40179a68` | Timer command builder |
| `0x40179b64` | Target humidity command builder |
| `0x40179c5c` | Display command builder |
| `0x40179d38` | Manual mode command builder |
| `0x40179ea0` | Mist level command builder |
| `0x40179f7c` | Warm level command builder |
| `0x4017a074` | Humidity mode command builder |
| `0x4017a150` | Sleep mode command builder |
| `0x4017a33c` | Query MCU status command builder |
| `0x4017a500` | UART ACK/status dispatcher |
| `0x4017c440` | Humidifier UART queue put |
| `0x4017c4dc` | Humidifier UART queue dequeue/send |
| `0x4017c514` | Humidifier UART queue worker task |
| `0x401845dc` | VeSync UART send wrapper |
| `0x40184914` | A5 transport frame send |
| `0x40184a7c` | A5 transport byte input |
| `0x401233a8` | Command body encoder |
| `0x401234ec` | A5 frame encoder/checksum |
| `0x401c5ca8` | Low-level UART init |
| `0x401c5d74` | Low-level UART send |
| `0x401c93d0` | Virtual mist level to real mist level mapping |
| `0x401c940c` | Real mist level to virtual mist level mapping |

## UART And Frame Format

The stock ESP32 initializes UART1 at 9600 baud, 8N1, with TX on GPIO17 and RX on
GPIO16. This matches the current ESPHome YAML.

The stock transport frame is:

```text
A5 CTRL SEQ LEN_L LEN_H CHECKSUM BODY...
```

The command body encoder writes:

```text
01 CMD_L CMD_H PARAM PAYLOAD...
```

The checksum is the ones-complement of the frame byte sum while treating the
checksum byte as zero. The command-send path uses control byte `0x22` when an ACK
is requested.

## Confirmed Command IDs

| Command | Meaning | Payload |
| --- | --- | --- |
| `0x4110` | Query status | Empty |
| `0xA000` | Power | `00` off, `01` on |
| `0xA105` | Display | `00` off, `0x64` on |
| `0xA2E8` | Target humidity | `00, target` where target is 40-80 |
| `0xA264` | Timer | `uint32_le seconds`, max 43,200 |
| `0xA265` | Timer remaining | `uint16_le remaining_seconds` in ACK/report payload |
| `0x4113` | Mist level | `level, 00, 00, 00` |
| `0x4112` | Warm level | `level, 00, 00, 00, 00` |
| `0xA260` | Manual mode level | `warm_enable, mist_enable, level` |
| `0x4114` | Humidity mode | `target, 00, 00, 00` |
| `0x4082` | Sleep mode | `target, 00, 00, 00, 00, 00` |
| `0xD101` | MCU reboot | One byte |
| `0xD007` | UART test | Empty |

The ACK dispatcher handles at least `0x4110`, `0xA265`, `0xA003`, `0xA2E8`,
`0xA260`, `0x4112`, and `0x4113`.

## Status Payload

The `0x4110` response contains a 20-byte MCU status payload after the 4-byte body
header. The decoded fields are:

| Offset | Meaning |
| --- | --- |
| `0` | MCU/protocol version byte 0 |
| `1` | MCU/protocol version byte 1 |
| `2` | MCU/protocol version byte 2 |
| `3` | Power/enabled |
| `4` | Container/tank state |
| `5` | Water lacks |
| `6` | Display config |
| `7` | Raw fog status |
| `8` | Target humidity |
| `9` | Current humidity |
| `10` | Current temperature |
| `11` | Raw MCU mode |
| `12` | Mist level |
| `13` | Warm level |
| `14` | Reserved/unknown byte before other exception |
| `15` | Other exception |

Stock firmware derives these values from the raw bytes:

| State | Stock derivation |
| --- | --- |
| Display on | Power is on, display config is nonzero, and raw mode is not sleep |
| Tank removed | Container byte is nonzero |
| Humidifying | On the tested MCU, power is on, water/tank are OK, and raw fog status is `0` |
| Warm enabled | Warm level is nonzero |
| Timer remaining | First two payload bytes from `0xA265`, little-endian |

The current ESPHome component now mirrors these derivations and exposes the raw
diagnostic bytes that are useful while testing.

## Mode Behavior

The stock app-side modes are:

| App mode | String |
| --- | --- |
| `0` | `auto` |
| `1` | `humidity` |
| `2` | `sleep` |
| `3` | `sleep` |
| `4` | `manual` |

The raw MCU status mode byte is remapped before stock firmware reports app state:

| Raw MCU mode | App-side mode |
| --- | --- |
| `0` | `0` / `auto` |
| `1` | `4` / `manual` |
| `2` | `2` / `sleep`, also forces display off |
| `3` | `1` / `humidity` |
| Other | `0` / `auto` |

The stock work-mode setter has UART paths for manual, humidity, and sleep:

| Requested string | UART behavior |
| --- | --- |
| `manual` | Sets app mode 4, sends `0xA260` |
| `humidity` | Sets app mode 1, sends `0x4114` |
| `sleep` | Sets app mode 2, sends `0x4082` |
| `auto` | Sets app mode 0 locally; no separate direct MCU command was found in this path |

This explains why the current ESPHome `Mode` select is still optimistic. The MCU
status byte is authoritative for diagnostics, but it is not a direct match for
the Home Assistant select options.

## Level Behavior

Mist level has two scales in stock firmware:

| Virtual level | Real MCU mist level |
| --- | --- |
| `1` | `1` |
| `2` | `5` |
| `3` | `9` |

The inverse mapping is:

| Real MCU mist level | Virtual level |
| --- | --- |
| `1-3` | `1` |
| `4-6` | `2` |
| `7-9` | `3` |

ESPHome currently exposes the real 1-9 MCU mist level, which is the most direct
replacement-firmware control. A separate virtual 1-3 control could be added later
as a Home Assistant convenience layer.

Warm level is 0-3. Stock firmware treats warm level 0 as warm disabled and levels
1-3 as warm enabled.

## Stock Cloud/App Features

The firmware includes stock cloud/app features that are not required for the MCU
replacement protocol:

| Feature | Notes |
| --- | --- |
| Schedules | Stored in ESP flash/user config and exposed as `extension.schedule_count` |
| Timer remaining | Exposed to cloud as `extension.timer_remain`; implemented in ESPHome as diagnostic state |
| Virtual mist level | Cloud reports both `mist_virtual_level` and `mist_level` |
| Automatic stop / humidity high | App/cloud state derived inside the stock ESP32 firmware |
| Analytics/history | Stock firmware reports events like mode, display, warm level, timer, and schedules |
| Production commands | Reboot, UART test, and production result paths exist; they should remain diagnostic only |

The full stock status JSON includes `enabled`, `mist_virtual_level`, `mist_level`,
`mode`, `water_lacks`, `water_tank_lifted`, `humidity`, `humidity_high`,
`display`, `warm_enabled`, `warm_level`, `automatic_stop_reach_target`,
`configuration.auto_target_humidity`, `configuration.display`,
`extension.schedule_count`, and `extension.timer_remain`.

## Stock Command Guards

The cloud/API layer rejects some requests before it sends UART commands:

| Request | Stock guard behavior |
| --- | --- |
| Target humidity | Requires power on, no water/tank fault, not manual, not sleep, target 40-80 |
| Mode | Requires no water/tank fault; accepts `auto`, `manual`, `sleep`, and `humidity` strings |
| Mist/warm level | Rejects water/tank fault and sleep mode |
| Relative level changes | Rejects auto mode |
| Display | Requires power on and rejects sleep mode |

ESPHome currently sends commands directly and then reconciles state from the MCU.
Adding these guards to the template controls would make the UI more stock-like,
but it is not required for protocol compatibility.

## ESPHome Coverage

Already covered in the current component/YAML:

- UART1 GPIO17/GPIO16 at 9600 8N1.
- A5 frame encoder/decoder and checksum.
- Power, display, target humidity, timer, mist, warm, manual mode, humidity mode,
  sleep mode, status query, MCU reboot, and UART test command IDs.
- Status offsets 0-15, including display config, raw mode, reserved byte 14, and
  other exception.
- Derived display state and warm-enabled state.
- Timer remaining from `0xA265`.
- ACK/result diagnostics.

Not implemented intentionally:

- Stock cloud schedule engine.
- Stock analytics/history reporting.
- Stock cloud error responses and command guards.
- Separate virtual 1-3 mist-level UI.
- Normal user-facing production commands.
