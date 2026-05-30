/*
 * sht3x.h
 *
 *  Created on: 29.05.2026
 *      Author: User
 */

#ifndef INC_SHT3X_H_
#define INC_SHT3X_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SHT3X_I2C_ADDR_7BIT_DEFAULT   (0x44u)

typedef enum
{
  SHT3X_STATUS_OK = 0,
  SHT3X_STATUS_ERROR,
  SHT3X_STATUS_INVALID_ARG,
  SHT3X_STATUS_NOT_READY,
  SHT3X_STATUS_TX_ERROR,
  SHT3X_STATUS_RX_ERROR,
  SHT3X_STATUS_CRC_ERROR
} sht3x_status_t;

typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint16_t address_hal;
} sht3x_t;

typedef struct
{
  uint16_t raw_temperature_ticks;
  uint16_t raw_humidity_ticks;

  float temperature_c;
  float humidity_rh;
} sht3x_sample_t;
/**
 * @brief Initializes the SHT3x driver context.
 */
void sht3x_init(sht3x_t *dev, I2C_HandleTypeDef *hi2c);

/**
 * @brief Checks whether the SHT3x responds on the I2C bus.
 */
sht3x_status_t sht3x_is_ready(sht3x_t *dev);

/**
 * @brief Reads one temperature/humidity sample using single-shot mode.
 *
 * This helper is intended for blocking I2C transfers and is useful for bring-up tests
 */
sht3x_status_t sht3x_read_sample_blocking(sht3x_t *dev, sht3x_sample_t *sample);

/**
 * @brief Builds the SHT3x single-shot measurement command.
 *
 * This helper is intended for interrupt-based I2C transfers where the caller
 * owns the TX buffer.
 */
sht3x_status_t sht3x_build_measure_command(uint8_t *tx_buf, uint16_t tx_buf_size, uint16_t *tx_len);

/**
 * @brief Parses a 6-byte SHT3x measurement response.
 *
 * Expected response format:
 *   temperature MSB, temperature LSB, temperature CRC,
 *   humidity MSB, humidity LSB, humidity CRC.
 */
sht3x_status_t sht3x_parse_sample_response(const uint8_t *rx_buf, uint16_t rx_len, sht3x_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* INC_SHT3X_H_ */
