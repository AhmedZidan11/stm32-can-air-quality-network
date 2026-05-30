/*
 * bxcan_if.c
 *
 *  Created on: 26.05.2026
 *      Author: User
 */

#include <stddef.h>
#include "bxcan_if.h"

can_if_status_t BXCAN_IF_Init(bxcan_if_t *ctx, CAN_HandleTypeDef *hcan)
{
    if ((ctx == NULL) || (hcan == NULL)) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    ctx->hcan = hcan;
    ctx->tx_done = 0U;
    ctx->rx_ready = 0U;
    ctx->latest_rx_frame.id = 0U;
    ctx->latest_rx_frame.id_type = CAN_ID_STANDARD;
    ctx->latest_rx_frame.frame_type = CAN_FRAME_DATA;
    ctx->latest_rx_frame.dlc = 0U;

    for (uint8_t i = 0U; i < CAN_MAX_DLC; i++) {
        ctx->latest_rx_frame.data[i] = 0U;
    }

    return CAN_IF_OK;
}

static can_if_status_t BXCAN_IF_Send(void *context, const can_frame_t *frame)
{
    bxcan_if_t *ctx = (bxcan_if_t *)context;

    if ((ctx == NULL) || (ctx->hcan == NULL) || (frame == NULL)) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t tx_mailbox = 0U;

    tx_header.DLC = frame->dlc;
    tx_header.IDE = (frame->id_type == CAN_ID_EXTENDED) ? CAN_ID_EXT : CAN_ID_STD;
    tx_header.RTR = (frame->frame_type == CAN_FRAME_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    tx_header.TransmitGlobalTime = DISABLE;

    if (frame->id_type == CAN_ID_EXTENDED) {
        tx_header.ExtId = frame->id;
    } else {
        tx_header.StdId = frame->id;
    }

    if (HAL_CAN_AddTxMessage(ctx->hcan, &tx_header, (uint8_t *)frame->data, &tx_mailbox) != HAL_OK) {
        return CAN_IF_ERROR_BACKEND;
    }

    return CAN_IF_OK;
}

static uint8_t BXCAN_IF_Read(void *context, can_frame_t *out_frame)
{
    bxcan_if_t *ctx = (bxcan_if_t *)context;

    if ((ctx == NULL) || (out_frame == NULL)) {
        return 0U;
    }

    if (ctx->rx_ready == 0U) {
        return 0U;
    }

    __disable_irq();
    *out_frame = ctx->latest_rx_frame;
    ctx->rx_ready = 0U;
    __enable_irq();

    return 1U;
}

static uint8_t BXCAN_IF_Tx_Done(void *context)
{
    bxcan_if_t *ctx = (bxcan_if_t *)context;
    uint8_t done = 0U;

    if (ctx == NULL) {
        return 0U;
    }

    __disable_irq();

    if (ctx->tx_done != 0U) {
        ctx->tx_done = 0U;
        done = 1U;
    }

    __enable_irq();

    return done;
}

static uint8_t BXCAN_IF_Is_Busy(void *context)
{
    bxcan_if_t *ctx = (bxcan_if_t *)context;

    if ((ctx == NULL) || (ctx->hcan == NULL)) {
        return 1U;
    }

    return (HAL_CAN_GetTxMailboxesFreeLevel(ctx->hcan) == 0U) ? 1U : 0U;
}

can_if_status_t BXCAN_IF_Get_CAN_IF_Backend(bxcan_if_t *ctx, can_if_backend_t *backend)
{
    if ((ctx == NULL) || (backend == NULL)) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    backend->context = ctx;
    backend->send = BXCAN_IF_Send;
    backend->read = BXCAN_IF_Read;
    backend->tx_done = BXCAN_IF_Tx_Done;
    backend->is_busy = BXCAN_IF_Is_Busy;

    return CAN_IF_OK;
}

void BXCAN_IF_On_Tx_Complete(bxcan_if_t *ctx, CAN_HandleTypeDef *hcan)
{
    if ((ctx == NULL) || (ctx->hcan == NULL) || (hcan == NULL)) {
        return;
    }

    if (hcan->Instance != ctx->hcan->Instance) {
        return;
    }

    ctx->tx_done = 1U;
}

void BXCAN_IF_On_Rx_Fifo0_Message_Pending(bxcan_if_t *ctx, CAN_HandleTypeDef *hcan)
{
    if ((ctx == NULL) || (ctx->hcan == NULL) || (hcan == NULL)) {
        return;
    }

    if (hcan->Instance != ctx->hcan->Instance) {
        return;
    }

    CAN_RxHeaderTypeDef rx_header = {0};
    uint8_t rx_data[CAN_MAX_DLC] = {0};

    if (HAL_CAN_GetRxMessage(ctx->hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK) {
        return;
    }

    if (rx_header.IDE == CAN_ID_EXT) {
        ctx->latest_rx_frame.id_type = CAN_ID_EXTENDED;
        ctx->latest_rx_frame.id = rx_header.ExtId;
    } else {
        ctx->latest_rx_frame.id_type = CAN_ID_STANDARD;
        ctx->latest_rx_frame.id = rx_header.StdId;
    }

    if (rx_header.RTR == CAN_RTR_REMOTE) {
        ctx->latest_rx_frame.frame_type = CAN_FRAME_REMOTE;
    } else {
        ctx->latest_rx_frame.frame_type = CAN_FRAME_DATA;
    }

    ctx->latest_rx_frame.dlc = rx_header.DLC;

    if (ctx->latest_rx_frame.dlc > CAN_MAX_DLC) {
        ctx->latest_rx_frame.dlc = CAN_MAX_DLC;
    }

    for (uint8_t i = 0U; i < CAN_MAX_DLC; i++) {
        ctx->latest_rx_frame.data[i] = 0U;
    }

    if (ctx->latest_rx_frame.frame_type == CAN_FRAME_DATA) {
        for (uint8_t i = 0U; i < ctx->latest_rx_frame.dlc; i++) {
            ctx->latest_rx_frame.data[i] = rx_data[i];
        }
    }

    ctx->rx_ready = 1U;
}
