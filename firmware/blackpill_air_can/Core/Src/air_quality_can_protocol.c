/*
 * air_quality_can_protocol.c
 *
 *  Created on: 31.05.2026
 *      Author: User
 */

#include "air_quality_can_protocol.h"

static void aq_can_write_u16_le(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static uint16_t aq_can_read_u16_le(const uint8_t *src)
{
  return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8U));
}

uint8_t AQ_CAN_MakeStatusFlags(uint8_t sht3x_ok, uint8_t sgp40_ok, uint8_t voc_valid, uint8_t data_stale)
{
  uint8_t flags = (uint8_t)((AQ_CAN_PROTOCOL_VERSION << AQ_CAN_STATUS_VERSION_SHIFT) & AQ_CAN_STATUS_VERSION_MASK);

  if (sht3x_ok != 0U)
  {
    flags |= AQ_CAN_STATUS_SHT3X_OK;
  }

  if (sgp40_ok != 0U)
  {
    flags |= AQ_CAN_STATUS_SGP40_OK;
  }

  if (voc_valid != 0U) {
    flags |= AQ_CAN_STATUS_VOC_VALID;
  }

  if (data_stale != 0U)
  {
    flags |= AQ_CAN_STATUS_DATA_STALE;
  }

  return flags;
}

uint8_t AQ_CAN_IsMeasurementFrame(const can_frame_t *frame)
{
  if (frame == 0)
  {
    return 0U;
  }

  if (frame->id_type != CAN_ID_STANDARD)
  {
    return 0U;
  }

  if (frame->frame_type != CAN_FRAME_DATA)
  {
    return 0U;
  }

  if (frame->id != AQ_CAN_MEASUREMENT_ID)
  {
    return 0U;
  }

  if (frame->dlc != AQ_CAN_MEASUREMENT_DLC)
  {
    return 0U;
  }

  return 1U;
}

can_if_status_t AQ_CAN_PackMeasurementFrame(const aq_can_measurement_t *measurement, can_frame_t *frame)
{
  if ((measurement == 0) || (frame == 0))
  {
    return CAN_IF_ERROR_INVALID_ARG;
  }

  frame->id = AQ_CAN_MEASUREMENT_ID;
  frame->id_type = CAN_ID_STANDARD;
  frame->frame_type = CAN_FRAME_DATA;
  frame->dlc = AQ_CAN_MEASUREMENT_DLC;

  aq_can_write_u16_le(&frame->data[0], (uint16_t)measurement->temperature_c_x100);
  aq_can_write_u16_le(&frame->data[2], measurement->humidity_rh_x100);
  aq_can_write_u16_le(&frame->data[4], measurement->voc_index);

  frame->data[6] = measurement->sample_counter;
  frame->data[7] = measurement->status_flags;

  return CAN_IF_OK;
}

can_if_status_t AQ_CAN_UnpackMeasurementFrame(const can_frame_t *frame, aq_can_measurement_t *measurement)
{
  if ((frame == 0) || (measurement == 0))
  {
    return CAN_IF_ERROR_INVALID_ARG;
  }

  if (AQ_CAN_IsMeasurementFrame(frame) == 0U)
  {
    return CAN_IF_ERROR_INVALID_ARG;
  }

  uint8_t version = (uint8_t)((frame->data[7] & AQ_CAN_STATUS_VERSION_MASK) >> AQ_CAN_STATUS_VERSION_SHIFT);

  if (version != AQ_CAN_PROTOCOL_VERSION)
  {
    return CAN_IF_ERROR_INVALID_ARG;
  }

  measurement->temperature_c_x100 = (int16_t)aq_can_read_u16_le(&frame->data[0]);
  measurement->humidity_rh_x100 = aq_can_read_u16_le(&frame->data[2]);
  measurement->voc_index = aq_can_read_u16_le(&frame->data[4]);
  measurement->sample_counter = frame->data[6];
  measurement->status_flags = frame->data[7];

  return CAN_IF_OK;
}
