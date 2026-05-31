/*
 * can_if.h
 *
 *  Created on: 21.05.2026
 *      Author: User
 */

#ifndef INC_CAN_IF_H_
#define INC_CAN_IF_H_


#include <stdint.h>

#define CAN_MAX_DLC             8U
#define CAN_STANDARD_ID_MAX     0x7FFU
#define CAN_EXTENDED_ID_MAX     0x1FFFFFFFU

typedef enum {
    CAN_ID_STANDARD = 0,
    CAN_ID_EXTENDED
} can_id_type_t;

typedef enum {
    CAN_FRAME_DATA = 0,
    CAN_FRAME_REMOTE
} can_frame_type_t;

typedef enum {
    CAN_IF_OK = 0,
    CAN_IF_ERROR_INVALID_ARG,
    CAN_IF_ERROR_NOT_READY,
    CAN_IF_ERROR_BUSY,
    CAN_IF_ERROR_BACKEND
} can_if_status_t;

typedef struct {
    uint32_t id;
    can_id_type_t id_type;
    can_frame_type_t frame_type;
    uint8_t dlc;
    uint8_t data[CAN_MAX_DLC];
} can_frame_t;

typedef can_if_status_t (*can_if_send_fn_t)(void *context, const can_frame_t *frame);

typedef uint8_t (*can_if_read_fn_t)(void *context, can_frame_t *out_frame);

typedef uint8_t (*can_if_tx_done_fn_t)(void *context);

typedef uint8_t (*can_if_is_busy_fn_t)(void *context);

/**
 * @brief Backend function table for a concrete CAN implementation.
 *
 * A backend may be MCP2515 over SPI, STM32 internal CAN, or another CAN driver.
 * The generic CAN_IF layer calls these functions without knowing the hardware.
 */
typedef struct {
    void *context;
    can_if_send_fn_t send;
    can_if_read_fn_t read;
    can_if_tx_done_fn_t tx_done;
    can_if_is_busy_fn_t is_busy;
} can_if_backend_t;

/**
 * @brief Generic CAN interface object used by the application.
 *
 * The object stores the selected backend and initialization state.
 */
typedef struct {
    can_if_backend_t backend;
    uint8_t initialized;
} can_if_t;

/**
 * @brief Initialize a generic CAN interface with a concrete backend.
 */
can_if_status_t CAN_IF_Init(can_if_t *iface, const can_if_backend_t *backend);

/**
 * @brief Send one CAN frame through the configured backend.
 */
can_if_status_t CAN_IF_Send(can_if_t *iface, const can_frame_t *frame);

/**
 * @brief Read one received CAN frame if available.
 */
uint8_t CAN_IF_Read(can_if_t *iface,  can_frame_t *out_frame);

/**
 * @brief Take and clear the transmit-done event from the backend.
 */
uint8_t CAN_IF_Take_Tx_Done(can_if_t *iface);

/**
 * @brief Check whether the configured backend is currently busy.
 */
uint8_t CAN_IF_Is_Busy(can_if_t *iface);

#endif /* INC_CAN_IF_H_ */
