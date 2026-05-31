/*
 * MCP2515.h
 *
 *  Created on: 18.05.2026
 *      Author: User
 */

#ifndef INC_MCP2515_H_
#define INC_MCP2515_H_

#include "can_if.h"
#include "stm32f4xx_hal.h"


/* EFLG bits */
#define MCP2515_ERR_EWARN     0x01U
#define MCP2515_ERR_RXWAR     0x02U
#define MCP2515_ERR_TXWAR     0x04U
#define MCP2515_ERR_RXEP      0x08U
#define MCP2515_ERR_TXEP      0x10U
#define MCP2515_ERR_TXBO      0x20U
#define MCP2515_ERR_RX0OVR    0x40U
#define MCP2515_ERR_RX1OVR    0x80U



/* MCP2515 status after initialization */
typedef enum {
    MCP2515_OK = 0,
    MCP2515_ERROR_CONFIG_MODE,
    MCP2515_ERROR_REQUESTED_MODE,
    MCP2515_ERROR_INVALID_ARG,
    MCP2515_ERROR_BUSY,
    MCP2515_ERROR_SPI,
	MCP2515_ERROR_NOT_READY,
} MCP2515_Status_t;

typedef enum {
    MCP2515_MODE_NORMAL      = 0x00U,
    MCP2515_MODE_SLEEP       = 0x20U,
    MCP2515_MODE_LOOPBACK    = 0x40U,
    MCP2515_MODE_LISTEN_ONLY = 0x60U,
    MCP2515_MODE_CONFIG      = 0x80U
} MCP2515_Mode_t;

/**
 * @brief Initialize the MCP2515 driver and configure the controller.
 */
MCP2515_Status_t MCP2515_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, MCP2515_Mode_t mode);

/**
 * @brief Change MCP2515 operation mode after initialization.
 */
MCP2515_Status_t MCP2515_Set_Mode(MCP2515_Mode_t mode);

/**
 * @brief Start non-blocking transmission of one CAN frame.
 */
MCP2515_Status_t MCP2515_Send_Frame_IT(const can_frame_t *msg);

/**
 * @brief Notify the MCP2515 driver that the INT pin generated an EXTI event.
 *
 * Call this from HAL_GPIO_EXTI_Callback() when the configured MCP2515 INT pin
 * triggers. The function starts interrupt-flag servicing if the driver is idle.
 */
void MCP2515_EXTI_Handler(void);

/**
 * @brief Continue the MCP2515 internal state machine after an SPI transfer completes.
 *
 * Call this from HAL_SPI_TxRxCpltCallback(). The driver uses it to finish
 * TX load, RTS, RX buffer reads, interrupt flag clearing, and error-flag reads.
 */
void MCP2515_SPI_TxRxCplt_Handler(SPI_HandleTypeDef *hspi);

/**
 * @brief Read the latest received CAN frame if available.
 */
uint8_t MCP2515_Read_Frame(can_frame_t *out_msg);

/**
 * @brief Take and clear the transmit-done event flag.
 */
uint8_t MCP2515_Take_Tx_Done(void);

/**
 * @brief Configure one global ID filter rule using filter and mask values.
 */
MCP2515_Status_t MCP2515_Config_Id_Filter(MCP2515_Mode_t restore_mode, uint32_t filter_id, uint32_t mask_id, can_id_type_t id_type);

/**
 * @brief Configure MCP2515 to accept all CAN frames.
 */
MCP2515_Status_t MCP2515_Config_Accept_All(MCP2515_Mode_t restore_mode);

/**
 * @brief Take and clear the latest MCP2515 error flags.
 */
uint8_t MCP2515_Take_Error_Flags(uint8_t *out_error_flags);

/**
 * @brief Check whether the internal MCP2515 driver state machine is busy.
 */
uint8_t MCP2515_Is_Busy(void);

/**
 * @brief Fill a CAN interface backend table using the MCP2515 driver.
 */
MCP2515_Status_t MCP2515_Get_CAN_IF_Backend(can_if_backend_t *backend);

#endif /* INC_MCP2515_H_ */
