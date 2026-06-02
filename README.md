# STM32 CAN Air Quality Network

This STM32 project implements Classic CAN to transmit sensor measurements between two nodes. The goal of this project is to build a live air-quality estimator and send readings from a sensor node to a receiver node.

The first node contains an STM32F411CEU6 (a.k.a. BlackPill) and a CAN controller (MCP2515). It reads temperature, humidity, and VOC-related gas data from SHT3x-DIS and SGP40 sensors over I2C and calculates a VOC Index using Sensirion's Gas Index Algorithm. The resulting measurements are packed into a CAN frame and sent over Classic CAN through the external MCP2515 controller.

The second node contains the Nucleo-F446RE board and a CAN transceiver (SN65HVD230). It receives the CAN frame, decodes the payload, and prints the measured values over UART2.

## Measured Values

The current implementation transmits three air-quality-related values:

| Value | Sensor / Source | Meaning |
|---|---|---|
| Temperature | SHT3x-DIS | Ambient temperature in degrees Celsius |
| Relative humidity | SHT3x-DIS | Air humidity in percent relative humidity |
| VOC Index | SGP40 + Sensirion algorithm | Relative indicator for indoor VOC pollution changes |

VOC stands for **Volatile Organic Compounds**. These are organic chemical compounds that can evaporate into the air at room temperature. Typical indoor sources include cleaning products, paints, adhesives, furniture, cooking, and human activity.

The SGP40 does not directly output an absolute gas concentration in ppm. It provides a raw VOC-related signal. In this project, that signal is processed with Sensirion's Gas Index Algorithm to calculate a **VOC Index**, which is used as a relative indicator for changes in indoor air quality.

For more information:

- [Sensirion SGP40 product page](https://sensirion.com/products/catalog/SGP40)
- [Sensirion SGP40 datasheet](https://sensirion.com/media/documents/296373BB/6203C5DF/Sensirion_Gas_Sensors_Datasheet_SGP40.pdf)
- [Sensirion SHT3x-DIS datasheet](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)

## Current Baseline

- Live SHT3x-DIS temperature and humidity measurement
- Live SGP40 raw VOC measurement
- VOC Index calculation using Sensirion Gas Index Algorithm
- Non-blocking air-quality service using I2C interrupt/state-machine logic
- Classic CAN transmission through MCP2515
- CAN reception on Nucleo-F446RE using internal bxCAN
- UART2 output of decoded air-quality values
- Compact 8-byte CAN payload using scaled integers

## Hardware Overview

### Sensor Node

| Component | Purpose |
|---|---|
| STM32F411 BlackPill | Sensor acquisition, VOC processing, and CAN transmission |
| SHT3x-DIS | Digital temperature and relative humidity sensor |
| SGP40 | Digital VOC-related gas sensor |
| MCP2515 module | External SPI-to-CAN controller |
| CAN transceiver | Physical CAN bus interface |

### Receiver Node

| Component | Purpose |
|---|---|
| Nucleo-F446RE | CAN receiver and UART monitor |
| SN65HVD230 | CAN transceiver |
| UART2 / ST-LINK VCP | PC terminal output |

## Documentation

| File | Content |
|---|---|
| [`docs/system_overview.md`](docs/system_overview.md) | Short technical overview of the hardware structure, sensors, firmware modules, and data flow between both nodes |
| [`docs/can_protocol.md`](docs/can_protocol.md) | Definition of the CAN frame ID, payload layout, scaling of physical values, byte order, and status flags |
| [`docs/test_results.md`](docs/test_results.md) | Initial working-baseline validation, including test setup notes, UART output sample, and observed limitations |

## Build and Flash

Both firmware projects are STM32CubeIDE projects.

Import the projects as existing projects:

```text
File > Import > General > Existing Projects into Workspace
```

Import these folders:

```text
firmware/blackpill_air_can
firmware/nucleo_can_uart
```

Do not copy the projects into the workspace. Keep them inside the repository folder.

When both ST-LINK probes are connected, select the correct ST-LINK serial number in each debug configuration.

## Current Limitations

* UART output is used as the first receiver-side display method.
* Error management and diagnostics of CAN frames are not fully implemented yet.
* Status flags are basic and need to be improved in the future.

