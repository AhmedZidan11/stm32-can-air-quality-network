/*
 * air_quality_service.h
 *
 *  Created on: 29.05.2026
 *      Author: User
 */

#ifndef INC_AIR_QUALITY_SERVICE_H_
#define INC_AIR_QUALITY_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "sht3x.h"
#include "sgp40.h"
#include "sensirion_gas_index_algorithm.h"
#include <stdint.h>

#define AIR_QUALITY_SAMPLE_PERIOD_MS    (1000u)

typedef enum
{
  AIR_QUALITY_STATUS_OK = 0,
  AIR_QUALITY_STATUS_NO_UPDATE,
  AIR_QUALITY_STATUS_INVALID_ARG,

  AIR_QUALITY_STATUS_SENSOR_NOT_READY,

  AIR_QUALITY_STATUS_SHT3X_READ_ERROR,
  AIR_QUALITY_STATUS_SGP40_READ_ERROR,
  AIR_QUALITY_STATUS_STATE_ERROR
} air_quality_status_t;

typedef enum
{
  AIR_QUALITY_STATE_IDLE = 0,

  AIR_QUALITY_STATE_SHT3X_TX,
  AIR_QUALITY_STATE_SHT3X_WAIT,
  AIR_QUALITY_STATE_SHT3X_RX,

  AIR_QUALITY_STATE_SGP40_TX,
  AIR_QUALITY_STATE_SGP40_WAIT,
  AIR_QUALITY_STATE_SGP40_RX,

  AIR_QUALITY_STATE_PROCESS
} air_quality_state_t;

typedef enum
{
  AIR_QUALITY_I2C_OP_NONE = 0,
  AIR_QUALITY_I2C_OP_SHT3X_TX,
  AIR_QUALITY_I2C_OP_SHT3X_RX,
  AIR_QUALITY_I2C_OP_SGP40_TX,
  AIR_QUALITY_I2C_OP_SGP40_RX
} air_quality_i2c_op_t;

typedef struct
{
  /* Low-level sensor drivers. */
  sgp40_t sgp40;
  sht3x_t sht3x;

  /* Sensirion VOC index algorithm state. */
  GasIndexAlgorithmParams voc_algorithm;

  /* Non-blocking measurement state machine. */
  air_quality_state_t state;
  volatile air_quality_i2c_op_t active_i2c_op;
  volatile uint8_t i2c_transfer_done;

  uint32_t sample_period_ms;
  uint32_t last_sample_tick;
  uint32_t state_entry_tick;
  uint32_t i2c_timeout_count;

  /* I2C transfer buffers owned by the service.
   * These buffers must persist after HAL_I2C_*_IT() returns.
   */
  uint8_t tx_buf[8];
  uint8_t rx_buf[6];
  uint16_t tx_len;
  uint16_t rx_len;

  /* Latest SHT3x measurement. */
  sht3x_sample_t sht3x_sample;
  uint16_t humidity_ticks;
  uint16_t temperature_ticks;
  float temperature_c;
  float humidity_rh;

  /* Latest SGP40 and VOC index results. */
  uint16_t sraw_voc;
  int32_t voc_index;
  uint8_t voc_index_valid;

  /* Last driver statuses. */
  sht3x_status_t last_sht3x_status;
  sgp40_status_t last_sgp40_status;

  /* Diagnostic counters. */
  uint32_t sht3x_read_ok_count;
  uint32_t sht3x_read_error_count;
  uint32_t voc_process_count;
  uint32_t read_ok_count;
  uint32_t read_error_count;
} air_quality_service_t;

/**
 * @brief Initializes the air quality service.
 *
 * The service owns the SHT3x and SGP40 sensor contexts, runs a non-blocking
 * I2C state machine, and feeds compensated SGP40 raw VOC samples into the
 * Sensirion VOC index algorithm.
 */
void air_quality_service_init(air_quality_service_t *service, I2C_HandleTypeDef *hi2c, uint32_t now_ms);

/**
 * @brief Checks whether both SHT3x and SGP40 respond on the I2C bus.
 */
air_quality_status_t air_quality_service_check_ready(air_quality_service_t *service);

/**
 * @brief Executes one non-blocking service step.
 *
 * Call this function frequently from the main loop. It starts a new measurement
 * cycle when the sample period has elapsed, advances pending I2C transfers
 * based on HAL callbacks, and updates temperature, humidity, SRAW_VOC, and
 * VOC index when a complete cycle finishes.
 */
air_quality_status_t air_quality_service_process(air_quality_service_t *service, uint32_t now_ms);

/**
 * @brief Notifies the service that an interrupt-driven I2C transmit finished.
 *
 * Call this function from HAL_I2C_MasterTxCpltCallback().
 */
void air_quality_service_on_i2c_tx_complete(air_quality_service_t *service, I2C_HandleTypeDef *hi2c);

/**
 * @brief Notifies the service that an interrupt-driven I2C receive finished.
 *
 * Call this function from HAL_I2C_MasterRxCpltCallback().
 */
void air_quality_service_on_i2c_rx_complete(air_quality_service_t *service, I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* INC_AIR_QUALITY_SERVICE_H_ */
