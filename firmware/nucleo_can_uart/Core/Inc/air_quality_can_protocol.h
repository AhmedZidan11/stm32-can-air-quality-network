/*
 * air_quality_can_protocol.h
 *
 *  Created on: 31.05.2026
 *      Author: User
 */

#ifndef INC_AIR_QUALITY_CAN_PROTOCOL_H_
#define INC_AIR_QUALITY_CAN_PROTOCOL_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "can_if.h"

#define AQ_CAN_MEASUREMENT_ID          (0x120U)
#define AQ_CAN_MEASUREMENT_DLC         (8U)
#define AQ_CAN_PROTOCOL_VERSION        (1U)

#define AQ_CAN_STATUS_SHT3X_OK         (1U << 0)
#define AQ_CAN_STATUS_SGP40_OK         (1U << 1)
#define AQ_CAN_STATUS_VOC_VALID        (1U << 2)
#define AQ_CAN_STATUS_DATA_STALE       (1U << 3)

#define AQ_CAN_STATUS_VERSION_SHIFT    (4U)
#define AQ_CAN_STATUS_VERSION_MASK     (0xF0U)

typedef struct
{
  int16_t  temperature_c_x100;
  uint16_t humidity_rh_x100;
  uint16_t voc_index;
  uint8_t  sample_counter;
  uint8_t  status_flags;
} aq_can_measurement_t;

uint8_t AQ_CAN_MakeStatusFlags(uint8_t sht3x_ok, uint8_t sgp40_ok, uint8_t voc_valid, uint8_t data_stale);

uint8_t AQ_CAN_IsMeasurementFrame(const can_frame_t *frame);

can_if_status_t AQ_CAN_PackMeasurementFrame(const aq_can_measurement_t *measurement, can_frame_t *frame);

can_if_status_t AQ_CAN_UnpackMeasurementFrame(const can_frame_t *frame, aq_can_measurement_t *measurement);

#ifdef __cplusplus
}
#endif

#endif /* INC_AIR_QUALITY_CAN_PROTOCOL_H_ */
