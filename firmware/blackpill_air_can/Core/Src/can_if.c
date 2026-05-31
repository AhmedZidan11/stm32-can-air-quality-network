/*
 * can_if.c
 *
 *  Created on: 25.05.2026
 *      Author: User
 */

#include <stddef.h>
#include "can_if.h"

can_if_status_t CAN_IF_Init(can_if_t *iface, const can_if_backend_t *backend)
{
    if ((iface == NULL) || (backend == NULL)) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    if ((backend->send == NULL) ||
        (backend->read == NULL) ||
        (backend->tx_done == NULL) ||
        (backend->is_busy == NULL)) {
        iface->initialized = 0U;
        return CAN_IF_ERROR_INVALID_ARG;
    }

    iface->backend = *backend;
    iface->initialized = 1U;

    return CAN_IF_OK;
}

static can_if_status_t CAN_IF_Validate_Frame(const can_frame_t *frame)
{
    if (frame == NULL) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    if (frame->dlc > CAN_MAX_DLC) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    if ((frame->frame_type != CAN_FRAME_DATA) &&
        (frame->frame_type != CAN_FRAME_REMOTE)) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    if (frame->id_type == CAN_ID_STANDARD) {
        if (frame->id > CAN_STANDARD_ID_MAX) {
            return CAN_IF_ERROR_INVALID_ARG;
        }
    } else if (frame->id_type == CAN_ID_EXTENDED) {
        if (frame->id > CAN_EXTENDED_ID_MAX) {
            return CAN_IF_ERROR_INVALID_ARG;
        }
    } else {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    return CAN_IF_OK;
}

can_if_status_t CAN_IF_Send(can_if_t *iface, const can_frame_t *frame)
{
    if (iface == NULL) {
        return CAN_IF_ERROR_INVALID_ARG;
    }

    can_if_status_t status = CAN_IF_Validate_Frame(frame);

    if (status != CAN_IF_OK) {
        return status;
    }

    if ((iface->initialized == 0U) || (iface->backend.send == NULL)) {
        return CAN_IF_ERROR_NOT_READY;
    }

    return iface->backend.send(iface->backend.context, frame);
}

uint8_t CAN_IF_Read(can_if_t *iface, can_frame_t *out_frame)
{
    if ((iface == NULL) || (out_frame == NULL)) {
        return 0U;
    }

    if ((iface->initialized == 0U) || (iface->backend.read == NULL)) {
        return 0U;
    }

    return iface->backend.read(iface->backend.context, out_frame);
}

uint8_t CAN_IF_Take_Tx_Done(can_if_t *iface)
{
    if (iface == NULL) {
        return 0U;
    }

    if ((iface->initialized == 0U) || (iface->backend.tx_done == NULL)) {
        return 0U;
    }

    return iface->backend.tx_done(iface->backend.context);
}

uint8_t CAN_IF_Is_Busy(can_if_t *iface)
{
    if (iface == NULL) {
        return 1U;
    }

    if ((iface->initialized == 0U) || (iface->backend.is_busy == NULL)) {
        return 1U;
    }

    return iface->backend.is_busy(iface->backend.context);
}
