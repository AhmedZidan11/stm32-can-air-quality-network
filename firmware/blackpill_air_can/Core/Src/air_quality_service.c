/*
 * air_quality_service.c
 *
 *  Created on: 29.05.2026
 *      Author: User
 */

#include "air_quality_service.h"

#define AIR_QUALITY_SHT3X_MEASUREMENT_DELAY_MS   (16u)
#define AIR_QUALITY_SGP40_MEASUREMENT_DELAY_MS   (30u)
#define AIR_QUALITY_I2C_TRANSFER_TIMEOUT_MS      (50u)


#define AIR_QUALITY_DEFAULT_HUMIDITY_TICKS       (0x8000u)
#define AIR_QUALITY_DEFAULT_TEMPERATURE_TICKS    (0x6666u)

static void air_quality_service_enter_state(air_quality_service_t *service, air_quality_state_t new_state, uint32_t now_ms);

static void air_quality_service_clear_i2c_transfer(air_quality_service_t *service);

static air_quality_status_t air_quality_service_fail_sht3x_cycle(air_quality_service_t *service, uint32_t now_ms);

static air_quality_status_t air_quality_service_fail_sgp40_cycle(air_quality_service_t *service, uint32_t now_ms);

void air_quality_service_init(air_quality_service_t *service, I2C_HandleTypeDef *hi2c, uint32_t now_ms)
{
  if (service == NULL)
  {
    return;
  }

  sgp40_init(&service->sgp40, hi2c);
  sht3x_init(&service->sht3x, hi2c);
  GasIndexAlgorithm_init(&service->voc_algorithm, GasIndexAlgorithm_ALGORITHM_TYPE_VOC);

  service->sample_period_ms = AIR_QUALITY_SAMPLE_PERIOD_MS;
  service->last_sample_tick = now_ms;

  service->state = AIR_QUALITY_STATE_IDLE;
  service->active_i2c_op = AIR_QUALITY_I2C_OP_NONE;
  service->i2c_transfer_done = 0u;

  service->state_entry_tick = now_ms;
  service->i2c_timeout_count = 0u;

  service->tx_len = 0u;
  service->rx_len = 0u;

  service->last_sht3x_status = SHT3X_STATUS_ERROR;

  service->humidity_ticks = AIR_QUALITY_DEFAULT_HUMIDITY_TICKS;
  service->temperature_ticks = AIR_QUALITY_DEFAULT_TEMPERATURE_TICKS;

  service->temperature_c = 0.0f;
  service->humidity_rh = 0.0f;

  service->sht3x_read_ok_count = 0u;
  service->sht3x_read_error_count = 0u;

  for (uint8_t i = 0u; i < sizeof(service->tx_buf); i++)
  {
    service->tx_buf[i] = 0u;
  }

  for (uint8_t i = 0u; i < sizeof(service->rx_buf); i++)
  {
    service->rx_buf[i] = 0u;
  }

  service->sraw_voc = 0u;
  service->voc_index = 0;
  service->voc_index_valid = 0u;

  service->voc_process_count = 0u;
  service->read_ok_count = 0u;
  service->read_error_count = 0u;

  service->last_sgp40_status = SGP40_STATUS_ERROR;
}

air_quality_status_t air_quality_service_check_ready(air_quality_service_t *service)
{
  if (service == NULL)
  {
	return AIR_QUALITY_STATUS_INVALID_ARG;
  }

  service->last_sht3x_status = sht3x_is_ready(&service->sht3x);
  service->last_sgp40_status = sgp40_is_ready(&service->sgp40);

  if ((service->last_sht3x_status == SHT3X_STATUS_OK) &&
	  (service->last_sgp40_status == SGP40_STATUS_OK))
  {
	return AIR_QUALITY_STATUS_OK;
  }

  return AIR_QUALITY_STATUS_SENSOR_NOT_READY;
}

air_quality_status_t air_quality_service_process(air_quality_service_t *service, uint32_t now_ms)
{
  int32_t voc_index = 0;

  if (service == NULL)
  {
    return AIR_QUALITY_STATUS_INVALID_ARG;
  }

  switch (service->state)
  {
    case AIR_QUALITY_STATE_IDLE:
    {
      if ((now_ms - service->last_sample_tick) < service->sample_period_ms)
      {
        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      service->last_sample_tick = now_ms;

      service->last_sht3x_status = sht3x_build_measure_command(service->tx_buf, sizeof(service->tx_buf), &service->tx_len);

      if (service->last_sht3x_status != SHT3X_STATUS_OK)
      {
    	 return air_quality_service_fail_sht3x_cycle(service, now_ms);
      }

      service->i2c_transfer_done = 0u;
      service->active_i2c_op = AIR_QUALITY_I2C_OP_SHT3X_TX;

      HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit_IT(service->sht3x.hi2c, service->sht3x.address_hal, service->tx_buf, service->tx_len);

      if (tx_status != HAL_OK)
      {
        service->last_sht3x_status = SHT3X_STATUS_TX_ERROR;
        return air_quality_service_fail_sht3x_cycle(service, now_ms);
      }

      air_quality_service_enter_state(service, AIR_QUALITY_STATE_SHT3X_TX, now_ms);
      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_SHT3X_TX:
    {
      if (service->i2c_transfer_done != 0u)
      {
    	air_quality_service_clear_i2c_transfer(service);

        air_quality_service_enter_state(service, AIR_QUALITY_STATE_SHT3X_WAIT, now_ms);
        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      if ((now_ms - service->state_entry_tick) > AIR_QUALITY_I2C_TRANSFER_TIMEOUT_MS)
      {
        service->i2c_timeout_count++;
        service->last_sht3x_status = SHT3X_STATUS_TX_ERROR;
        return air_quality_service_fail_sht3x_cycle(service, now_ms);
      }

      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_SHT3X_WAIT:
    {
      if ((now_ms - service->state_entry_tick) < AIR_QUALITY_SHT3X_MEASUREMENT_DELAY_MS)
      {
        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      service->rx_len = 6u;
      service->i2c_transfer_done = 0u;
      service->active_i2c_op = AIR_QUALITY_I2C_OP_SHT3X_RX;

      HAL_StatusTypeDef rx_status = HAL_I2C_Master_Receive_IT( service->sht3x.hi2c, service->sht3x.address_hal, service->rx_buf, service->rx_len);

      if (rx_status != HAL_OK)
      {
        service->last_sht3x_status = SHT3X_STATUS_RX_ERROR;
        return air_quality_service_fail_sht3x_cycle(service, now_ms);
      }

      air_quality_service_enter_state(service, AIR_QUALITY_STATE_SHT3X_RX, now_ms);
      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_SHT3X_RX:
    {
      if (service->i2c_transfer_done == 0u)
      {
        if ((now_ms - service->state_entry_tick) > AIR_QUALITY_I2C_TRANSFER_TIMEOUT_MS)
        {
          service->i2c_timeout_count++;
          service->last_sht3x_status = SHT3X_STATUS_RX_ERROR;
          return air_quality_service_fail_sht3x_cycle(service, now_ms);
        }

        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      air_quality_service_clear_i2c_transfer(service);

      service->last_sht3x_status = sht3x_parse_sample_response(service->rx_buf, service->rx_len, &service->sht3x_sample);

      if (service->last_sht3x_status != SHT3X_STATUS_OK)
      {
    	 return air_quality_service_fail_sht3x_cycle(service, now_ms);
      }

      service->temperature_ticks = service->sht3x_sample.raw_temperature_ticks;
      service->humidity_ticks = service->sht3x_sample.raw_humidity_ticks;

      service->temperature_c = service->sht3x_sample.temperature_c;
      service->humidity_rh = service->sht3x_sample.humidity_rh;

      service->sht3x_read_ok_count++;

      service->last_sgp40_status = sgp40_build_measure_raw_command_with_ticks(service->humidity_ticks, service->temperature_ticks, service->tx_buf, sizeof(service->tx_buf), &service->tx_len);

      if (service->last_sgp40_status != SGP40_STATUS_OK)
      {
    	 return air_quality_service_fail_sgp40_cycle(service, now_ms);
      }

      service->i2c_transfer_done = 0u;
      service->active_i2c_op = AIR_QUALITY_I2C_OP_SGP40_TX;

      HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit_IT(service->sgp40.hi2c, service->sgp40.address_hal, service->tx_buf, service->tx_len);

      if (tx_status != HAL_OK)
      {
        service->last_sgp40_status = SGP40_STATUS_TX_ERROR;
        return air_quality_service_fail_sgp40_cycle(service, now_ms);
      }

      air_quality_service_enter_state(service, AIR_QUALITY_STATE_SGP40_TX, now_ms);
      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_SGP40_TX:
    {
      if (service->i2c_transfer_done != 0u)
      {
        air_quality_service_clear_i2c_transfer(service);

        air_quality_service_enter_state(service, AIR_QUALITY_STATE_SGP40_WAIT, now_ms);
        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      if ((now_ms - service->state_entry_tick) > AIR_QUALITY_I2C_TRANSFER_TIMEOUT_MS)
      {
        service->i2c_timeout_count++;
        service->last_sgp40_status = SGP40_STATUS_TX_ERROR;
        return air_quality_service_fail_sgp40_cycle(service, now_ms);
      }

      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_SGP40_WAIT:
    {
      if ((now_ms - service->state_entry_tick) < AIR_QUALITY_SGP40_MEASUREMENT_DELAY_MS)
      {
        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      service->rx_len = 3u;
      service->i2c_transfer_done = 0u;
      service->active_i2c_op = AIR_QUALITY_I2C_OP_SGP40_RX;

      HAL_StatusTypeDef rx_status = HAL_I2C_Master_Receive_IT(service->sgp40.hi2c, service->sgp40.address_hal, service->rx_buf, service->rx_len);

      if (rx_status != HAL_OK)
      {
        service->last_sgp40_status = SGP40_STATUS_RX_ERROR;
        return air_quality_service_fail_sgp40_cycle(service, now_ms);
      }

      air_quality_service_enter_state(service, AIR_QUALITY_STATE_SGP40_RX, now_ms);
      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_SGP40_RX:
    {
      if (service->i2c_transfer_done == 0u)
      {
        if ((now_ms - service->state_entry_tick) > AIR_QUALITY_I2C_TRANSFER_TIMEOUT_MS)
        {
          service->i2c_timeout_count++;
          service->last_sgp40_status = SGP40_STATUS_RX_ERROR;
          return air_quality_service_fail_sgp40_cycle(service, now_ms);
        }

        return AIR_QUALITY_STATUS_NO_UPDATE;
      }

      air_quality_service_clear_i2c_transfer(service);

      service->last_sgp40_status = sgp40_parse_sraw_response(service->rx_buf, service->rx_len, &service->sraw_voc);

      if (service->last_sgp40_status != SGP40_STATUS_OK)
      {
    	  return air_quality_service_fail_sgp40_cycle(service, now_ms);
      }

      air_quality_service_enter_state(service, AIR_QUALITY_STATE_PROCESS, now_ms);
      return AIR_QUALITY_STATUS_NO_UPDATE;
    }

    case AIR_QUALITY_STATE_PROCESS:
    {
      GasIndexAlgorithm_process(&service->voc_algorithm, (int32_t)service->sraw_voc, &voc_index);

      service->voc_index = voc_index;
      service->voc_index_valid = 1u;
      service->voc_process_count++;

      service->read_ok_count++;

      air_quality_service_enter_state(service, AIR_QUALITY_STATE_IDLE, now_ms);

      return AIR_QUALITY_STATUS_OK;
    }

    default:
    {
    	air_quality_service_clear_i2c_transfer(service);
    	service->read_error_count++;
    	air_quality_service_enter_state(service, AIR_QUALITY_STATE_IDLE, now_ms);
    	return AIR_QUALITY_STATUS_STATE_ERROR;
    }
  }
}

void air_quality_service_on_i2c_tx_complete(air_quality_service_t *service, I2C_HandleTypeDef *hi2c)
{
  if ((service == NULL) || (hi2c == NULL))
  {
    return;
  }

  if (service->sgp40.hi2c != hi2c)
  {
    return;
  }

  switch (service->active_i2c_op)
  {
    case AIR_QUALITY_I2C_OP_SHT3X_TX:
    case AIR_QUALITY_I2C_OP_SGP40_TX:
      service->i2c_transfer_done = 1u;
      break;

    default:
      break;
  }
}

void air_quality_service_on_i2c_rx_complete(air_quality_service_t *service, I2C_HandleTypeDef *hi2c)
{
  if ((service == NULL) || (hi2c == NULL))
  {
    return;
  }

  if (service->sgp40.hi2c != hi2c)
  {
    return;
  }

  switch (service->active_i2c_op)
  {
    case AIR_QUALITY_I2C_OP_SHT3X_RX:
    case AIR_QUALITY_I2C_OP_SGP40_RX:
      service->i2c_transfer_done = 1u;
      break;

    default:
      break;
  }
}

static void air_quality_service_enter_state(air_quality_service_t *service, air_quality_state_t new_state, uint32_t now_ms)
{
  service->state = new_state;
  service->state_entry_tick = now_ms;
}

static void air_quality_service_clear_i2c_transfer(air_quality_service_t *service)
{
  service->active_i2c_op = AIR_QUALITY_I2C_OP_NONE;
  service->i2c_transfer_done = 0u;
}

static air_quality_status_t air_quality_service_fail_sht3x_cycle(air_quality_service_t *service, uint32_t now_ms)
{
  air_quality_service_clear_i2c_transfer(service);

  service->sht3x_read_error_count++;
  service->read_error_count++;

  air_quality_service_enter_state(service, AIR_QUALITY_STATE_IDLE, now_ms);

  return AIR_QUALITY_STATUS_SHT3X_READ_ERROR;
}

static air_quality_status_t air_quality_service_fail_sgp40_cycle(air_quality_service_t *service, uint32_t now_ms)
{
  air_quality_service_clear_i2c_transfer(service);

  service->read_error_count++;

  air_quality_service_enter_state(service, AIR_QUALITY_STATE_IDLE, now_ms);

  return AIR_QUALITY_STATUS_SGP40_READ_ERROR;
}
