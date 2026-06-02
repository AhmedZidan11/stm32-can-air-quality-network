# CAN Protocol

This document defines the current CAN frame used between the BlackPill sensor node and the Nucleo receiver node.

## Bus Configuration

| Item | Value |
|---|---|
| CAN type | Classic CAN |
| Bitrate | 125 kbit/s |
| Identifier type | Standard 11-bit ID |
| Payload size | 8 bytes |
| Multi-byte fields | Little-endian |

## Air Quality Measurement Frame

| Item | Value |
|---|---|
| CAN ID | `0x120` |
| DLC | `8` |
| Frame type | Standard data frame |
| Direction | BlackPill -> Nucleo |
| Rate | Approximately 1 frame per processed VOC sample |

## Payload Layout

| Byte | Field | Type | Scaling |
|---:|---|---|---|
| 0..1 | `temperature_c_x100` | `int16_t` | Temperature in °C × 100 |
| 2..3 | `humidity_rh_x100` | `uint16_t` | Relative humidity in %RH × 100 |
| 4..5 | `voc_index` | `uint16_t` | VOC Index |
| 6 | `sample_counter` | `uint8_t` | Increments per transmitted sample |
| 7 | `status_flags` | `uint8_t` | Status bits and protocol version |

## Scaling

### Temperature

The transmitted value is the temperature in degrees Celsius multiplied by 100.

Examples:

```text
2345  -> 23.45 °C
-520  -> -5.20 °C
```

### Relative Humidity

The transmitted value is the relative humidity percentage multiplied by 100.

Examples:

```text
5120 -> 51.20 %RH
4575 -> 45.75 %RH
```

### VOC Index

The VOC Index is transmitted as an unsigned integer.

Examples:

```text
100 -> VOC Index 100
250 -> VOC Index 250
```

## Status Flags

| Bit | Meaning |
|---:|---|
| 0 | SHT3x measurement valid |
| 1 | SGP40 measurement valid |
| 2 | VOC Index valid |
| 3 | Data stale |
| 4..7 | Protocol version |

Current protocol version:

```text
1
```

Typical valid status byte:

```text
0x17
```

Meaning:

```text
0x10 -> protocol version 1
0x01 -> SHT3x valid
0x02 -> SGP40 valid
0x04 -> VOC Index valid
```

## Example Frame

Decoded values:

| Field | Value |
|---|---:|
| Temperature | 23.45 °C |
| Relative humidity | 51.20 %RH |
| VOC Index | 100 |
| Sample counter | 0 |
| Status flags | 0x17 |

Raw payload:

```text
29 09 00 14 64 00 00 17
```

Field decoding:

```text
0x0929 = 2345 -> 23.45 °C
0x1400 = 5120 -> 51.20 %RH
0x0064 = 100  -> VOC Index 100
0x00   = sample counter
0x17   = status flags
```

## Implementation Files

The protocol pack/unpack helpers are implemented in both firmware projects:

```text
Core/Inc/air_quality_can_protocol.h
Core/Src/air_quality_can_protocol.c
```

The protocol layer avoids floating-point values in the CAN payload and uses explicit byte packing.