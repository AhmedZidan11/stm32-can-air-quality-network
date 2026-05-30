/*
 * sgp40.h
 *
 *  Created on: 28.05.2026
 *      Author: User
 */

#ifndef INC_SGP40_H_
#define INC_SGP40_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SGP40_I2C_ADDR_7BIT    (0x59u)

typedef enum
{
  SGP40_STATUS_OK = 0,
  SGP40_STATUS_ERROR,
  SGP40_STATUS_INVALID_ARG,
  SGP40_STATUS_NOT_READY,
  SGP40_STATUS_TX_ERROR,
  SGP40_STATUS_RX_ERROR,
  SGP40_STATUS_CRC_ERROR
} sgp40_status_t;

typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint16_t address_hal;
} sgp40_t;

/**
 * @brief Initializes the SGP40 driver context.
 */
void sgp40_init(sgp40_t *dev, I2C_HandleTypeDef *hi2c);

/**
 * @brief Checks whether the SGP40 responds on the I2C bus.
 */
sgp40_status_t sgp40_is_ready(sgp40_t *dev);

/**
 * @brief Reads one blocking raw VOC sample using default compensation values.
 *
 * This convenience function is useful for bring-up tests. The non-blocking
 * air_quality_service path builds the command with live SHT3x compensation ticks.
 */
sgp40_status_t sgp40_read_sraw_voc_blocking(sgp40_t *dev, uint16_t *sraw_voc);

/**
 * @brief Reads one blocking raw VOC sample using already encoded compensation ticks.
 *
 * The humidity_ticks and temperature_ticks are the 16-bit compensation values
 * expected by the SGP40 measurement command.
 */
sgp40_status_t sgp40_read_sraw_voc_with_ticks_blocking(sgp40_t *dev, uint16_t humidity_ticks, uint16_t temperature_ticks, uint16_t *sraw_voc);

/**
 * @brief Builds the SGP40 measure_raw command using already encoded
 * humidity and temperature ticks.
 *
 * Expected output length is 8 bytes.
 */
sgp40_status_t sgp40_build_measure_raw_command_with_ticks(uint16_t humidity_ticks, uint16_t temperature_ticks, uint8_t *tx_buf, uint16_t tx_buf_size, uint16_t *tx_len);

/**
 * @brief Parses a 3-byte SGP40 raw VOC response.
 *
 * Expected response format:
 *   SRAW MSB, SRAW LSB, CRC.
 */
sgp40_status_t sgp40_parse_sraw_response(const uint8_t *rx_buf, uint16_t rx_len, uint16_t *sraw_voc);

#ifdef __cplusplus
}
#endif

#endif /* INC_SGP40_H_ */
