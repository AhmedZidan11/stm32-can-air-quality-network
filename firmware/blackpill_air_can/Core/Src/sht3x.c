/*
 * sht3x.c
 *
 *  Created on: 29.05.2026
 *      Author: User
 */

#include "sht3x.h"

#define SHT3X_CMD_MEAS_HIGHREP_NOSTRETCH_MSB   (0x24u)
#define SHT3X_CMD_MEAS_HIGHREP_NOSTRETCH_LSB   (0x00u)

#define SHT3X_MEASUREMENT_DELAY_MS             (16u)
#define SHT3X_I2C_TIMEOUT_MS                   (100u)

#define SENSIRION_CRC8_INIT                    (0xFFu)
#define SENSIRION_CRC8_POLYNOMIAL              (0x31u)

static uint8_t sht3x_crc8(const uint8_t *data, uint16_t count);

void sht3x_init(sht3x_t *dev, I2C_HandleTypeDef *hi2c)
{
  if (dev == NULL)
  {
    return;
  }

  dev->hi2c = hi2c;
  dev->address_hal = (uint16_t)(SHT3X_I2C_ADDR_7BIT_DEFAULT << 1u);
}

sht3x_status_t sht3x_is_ready(sht3x_t *dev)
{
  if ((dev == NULL) || (dev->hi2c == NULL))
  {
    return SHT3X_STATUS_INVALID_ARG;
  }

  HAL_StatusTypeDef hal_status = HAL_I2C_IsDeviceReady(
      dev->hi2c,
      dev->address_hal,
      3u,
      SHT3X_I2C_TIMEOUT_MS);

  if (hal_status == HAL_OK)
  {
    return SHT3X_STATUS_OK;
  }

  return SHT3X_STATUS_NOT_READY;
}

sht3x_status_t sht3x_build_measure_command(uint8_t *tx_buf, uint16_t tx_buf_size, uint16_t *tx_len)
{
  if ((tx_buf == NULL) || (tx_len == NULL) || (tx_buf_size < 2u))
  {
    return SHT3X_STATUS_INVALID_ARG;
  }

  tx_buf[0] = SHT3X_CMD_MEAS_HIGHREP_NOSTRETCH_MSB;
  tx_buf[1] = SHT3X_CMD_MEAS_HIGHREP_NOSTRETCH_LSB;
  *tx_len = 2u;

  return SHT3X_STATUS_OK;
}

sht3x_status_t sht3x_parse_sample_response(const uint8_t *rx_buf, uint16_t rx_len, sht3x_sample_t *sample)
{
  if ((rx_buf == NULL) || (sample == NULL) || (rx_len < 6u))
  {
    return SHT3X_STATUS_INVALID_ARG;
  }

  if (sht3x_crc8(&rx_buf[0], 2u) != rx_buf[2])
  {
    return SHT3X_STATUS_CRC_ERROR;
  }

  if (sht3x_crc8(&rx_buf[3], 2u) != rx_buf[5])
  {
    return SHT3X_STATUS_CRC_ERROR;
  }

  const uint16_t raw_temperature = ((uint16_t)rx_buf[0] << 8u) | (uint16_t)rx_buf[1];

  const uint16_t raw_humidity = ((uint16_t)rx_buf[3] << 8u) | (uint16_t)rx_buf[4];

  sample->raw_temperature_ticks = raw_temperature;
  sample->raw_humidity_ticks = raw_humidity;

  sample->temperature_c = -45.0f + (175.0f * (float)raw_temperature / 65535.0f);

  sample->humidity_rh = 100.0f * (float)raw_humidity / 65535.0f;

  return SHT3X_STATUS_OK;
}

sht3x_status_t sht3x_read_sample_blocking(sht3x_t *dev, sht3x_sample_t *sample)
{
  if ((dev == NULL) || (dev->hi2c == NULL) || (sample == NULL))
  {
    return SHT3X_STATUS_INVALID_ARG;
  }

  uint8_t tx_buf[2] = {0u, 0u};
  uint16_t tx_len = 0u;
  uint8_t rx_buf[6] = {0u, 0u, 0u, 0u, 0u, 0u};

  sht3x_status_t build_status = sht3x_build_measure_command(tx_buf, sizeof(tx_buf), &tx_len);

  if (build_status != SHT3X_STATUS_OK)
  {
    return build_status;
  }

  HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit(
      dev->hi2c,
      dev->address_hal,
      tx_buf,
	  tx_len,
      SHT3X_I2C_TIMEOUT_MS);

  if (tx_status != HAL_OK)
  {
    return SHT3X_STATUS_TX_ERROR;
  }

  HAL_Delay(SHT3X_MEASUREMENT_DELAY_MS);

  HAL_StatusTypeDef rx_status = HAL_I2C_Master_Receive(
      dev->hi2c,
      dev->address_hal,
      rx_buf,
      sizeof(rx_buf),
      SHT3X_I2C_TIMEOUT_MS);

  if (rx_status != HAL_OK)
  {
    return SHT3X_STATUS_RX_ERROR;
  }

  return sht3x_parse_sample_response(rx_buf, sizeof(rx_buf), sample);
}

static uint8_t sht3x_crc8(const uint8_t *data, uint16_t count)
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
