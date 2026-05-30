/*
 * sgp40.c
 *
 *  Created on: 28.05.2026
 *      Author: User
 */

#include "sgp40.h"

#define SGP40_CMD_MEASURE_RAW_MSB       (0x26u)
#define SGP40_CMD_MEASURE_RAW_LSB       (0x0Fu)

#define SGP40_DEFAULT_RH_TICKS          (0x8000u)
#define SGP40_DEFAULT_TEMP_TICKS        (0x6666u)

#define SGP40_RAW_MEASUREMENT_DELAY_MS  (30u)
#define SGP40_I2C_TIMEOUT_MS            (100u)

#define SENSIRION_CRC8_INIT             (0xFFu)
#define SENSIRION_CRC8_POLYNOMIAL       (0x31u)

static uint8_t sgp40_crc8(const uint8_t *data, uint16_t count);

void sgp40_init(sgp40_t *dev, I2C_HandleTypeDef *hi2c)
{
  if (dev == NULL)
  {
    return;
  }

  dev->hi2c = hi2c;
  dev->address_hal = (uint16_t)(SGP40_I2C_ADDR_7BIT << 1u);
}

sgp40_status_t sgp40_is_ready(sgp40_t *dev)
{
  if ((dev == NULL) || (dev->hi2c == NULL))
  {
    return SGP40_STATUS_INVALID_ARG;
  }

  HAL_StatusTypeDef hal_status = HAL_I2C_IsDeviceReady(
      dev->hi2c,
      dev->address_hal,
      3u,
      SGP40_I2C_TIMEOUT_MS);

  if (hal_status == HAL_OK)
  {
    return SGP40_STATUS_OK;
  }

  return SGP40_STATUS_NOT_READY;
}

sgp40_status_t sgp40_build_measure_raw_command_with_ticks(uint16_t humidity_ticks, uint16_t temperature_ticks, uint8_t *tx_buf, uint16_t tx_buf_size, uint16_t *tx_len)
{
  if ((tx_buf == NULL) || (tx_len == NULL) || (tx_buf_size < 8u))
  {
    return SGP40_STATUS_INVALID_ARG;
  }

  const uint8_t humidity_word[2] =
  {
    (uint8_t)(humidity_ticks >> 8u),
    (uint8_t)(humidity_ticks & 0xFFu)
  };

  const uint8_t temperature_word[2] =
  {
    (uint8_t)(temperature_ticks >> 8u),
    (uint8_t)(temperature_ticks & 0xFFu)
  };

  tx_buf[0] = SGP40_CMD_MEASURE_RAW_MSB;
  tx_buf[1] = SGP40_CMD_MEASURE_RAW_LSB;

  tx_buf[2] = humidity_word[0];
  tx_buf[3] = humidity_word[1];
  tx_buf[4] = sgp40_crc8(humidity_word, 2u);

  tx_buf[5] = temperature_word[0];
  tx_buf[6] = temperature_word[1];
  tx_buf[7] = sgp40_crc8(temperature_word, 2u);

  *tx_len = 8u;

  return SGP40_STATUS_OK;
}

sgp40_status_t sgp40_parse_sraw_response(const uint8_t *rx_buf, uint16_t rx_len, uint16_t *sraw_voc)
{
  if ((rx_buf == NULL) || (sraw_voc == NULL) || (rx_len < 3u))
  {
    return SGP40_STATUS_INVALID_ARG;
  }

  const uint8_t expected_crc = sgp40_crc8(rx_buf, 2u);

  if (expected_crc != rx_buf[2])
  {
    return SGP40_STATUS_CRC_ERROR;
  }

  *sraw_voc = ((uint16_t)rx_buf[0] << 8u) | (uint16_t)rx_buf[1];

  return SGP40_STATUS_OK;
}

sgp40_status_t sgp40_read_sraw_voc_blocking(sgp40_t *dev, uint16_t *sraw_voc)
{
  return sgp40_read_sraw_voc_with_ticks_blocking(dev, SGP40_DEFAULT_RH_TICKS, SGP40_DEFAULT_TEMP_TICKS, sraw_voc);
}

sgp40_status_t sgp40_read_sraw_voc_with_ticks_blocking(sgp40_t *dev, uint16_t humidity_ticks, uint16_t temperature_ticks, uint16_t *sraw_voc)
{
  if ((dev == NULL) || (dev->hi2c == NULL) || (sraw_voc == NULL))
  {
    return SGP40_STATUS_INVALID_ARG;
  }

  uint8_t tx_buf[8] = {0u};
  uint16_t tx_len = 0u;
  uint8_t rx_buf[3] = {0u};

  sgp40_status_t build_status = sgp40_build_measure_raw_command_with_ticks(humidity_ticks, temperature_ticks, tx_buf, sizeof(tx_buf), &tx_len);

  if (build_status != SGP40_STATUS_OK)
  {
    return build_status;
  }

  HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit(dev->hi2c, dev->address_hal, tx_buf, tx_len, SGP40_I2C_TIMEOUT_MS);

  if (tx_status != HAL_OK)
  {
    return SGP40_STATUS_TX_ERROR;
  }

  HAL_Delay(SGP40_RAW_MEASUREMENT_DELAY_MS);

  HAL_StatusTypeDef rx_status = HAL_I2C_Master_Receive(dev->hi2c, dev->address_hal, rx_buf, sizeof(rx_buf), SGP40_I2C_TIMEOUT_MS);

  if (rx_status != HAL_OK)
  {
    return SGP40_STATUS_RX_ERROR;
  }

  return sgp40_parse_sraw_response(rx_buf, sizeof(rx_buf), sraw_voc);
}

static uint8_t sgp40_crc8(const uint8_t *data, uint16_t count)
{
  uint8_t crc = SENSIRION_CRC8_INIT;

  for (uint16_t i = 0u; i < count; i++)
  {
    crc ^= data[i];

    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
      if ((crc & 0x80u) != 0u)
      {
        crc = (uint8_t)((crc << 1u) ^ SENSIRION_CRC8_POLYNOMIAL);
      }
      else
      {
        crc <<= 1u;
      }
    }
  }

  return crc;
}

