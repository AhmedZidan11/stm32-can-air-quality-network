/*
 * bxcan_if.h
 *
 *  Created on: 26.05.2026
 *      Author: User
 */

#ifndef INC_BXCAN_IF_H_
#define INC_BXCAN_IF_H_

#include "stm32f4xx_hal.h"
#include "can_if.h"

typedef struct {
    CAN_HandleTypeDef *hcan;
    volatile uint8_t tx_done;
    volatile uint8_t rx_ready;
    can_frame_t latest_rx_frame;
} bxcan_if_t;

can_if_status_t BXCAN_IF_Init(bxcan_if_t *ctx, CAN_HandleTypeDef *hcan);

can_if_status_t BXCAN_IF_Get_CAN_IF_Backend(bxcan_if_t *ctx, can_if_backend_t *backend);

void BXCAN_IF_On_Tx_Complete(bxcan_if_t *ctx, CAN_HandleTypeDef *hcan);

void BXCAN_IF_On_Rx_Fifo0_Message_Pending(bxcan_if_t *ctx, CAN_HandleTypeDef *hcan);


#endif /* INC_BXCAN_IF_H_ */
