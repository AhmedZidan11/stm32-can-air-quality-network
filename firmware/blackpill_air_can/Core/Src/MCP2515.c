/*
 * MCP2515.c
 *
 *  Created on: 18.05.2026
 *      Author: User
 */

#include "MCP2515.h"

static SPI_HandleTypeDef *mcp_hspi;
static GPIO_TypeDef *mcp_cs_port;
static uint16_t mcp_cs_pin;
static volatile uint8_t mcp_initialized = 0U;
static volatile uint8_t mcp_int_pending = 0;
static volatile uint8_t mcp_int_flags = 0;


/* Operation modes: CANCTRL[7:5] / CANSTAT[7:5] */
#define MCP_MODE_MASK          0xE0

/* MCP2515 Commands */
#define MCP_RESET       	   0xC0
#define MCP_READ        	   0x03
#define MCP_WRITE       	   0x02
#define MCP_BITMOD      	   0x05
#define MCP_READ_RXB0   	   0x90
#define MCP_READ_RXB1          0x94
#define MCP_LOAD_TXB0   	   0x40
#define MCP_RTS_TX0     	   0x81

/* CANINTF / CANINTE bits */
#define MCP_INT_RX0IF          0x01
#define MCP_INT_RX1IF          0x02
#define MCP_INT_TX0IF          0x04
#define MCP_INT_TX1IF          0x08
#define MCP_INT_TX2IF          0x10
#define MCP_INT_ERRIF          0x20
#define MCP_INT_WAKIF          0x40
#define MCP_INT_MERRF          0x80
#define MCP_INT_ALL_FLAGS 	   0xFF

/* Common interrupt enable combinations */
#define MCP_INTE_RX0IE         MCP_INT_RX0IF
#define MCP_INTE_RX1IE         MCP_INT_RX1IF
#define MCP_INTE_TX0IE         MCP_INT_TX0IF
#define MCP_INTE_ERRIE         MCP_INT_ERRIF
#define MCP_INTE_MERRE         MCP_INT_MERRF

/* Addresses of MCP2515 Registers */
#define MCP_CANCTRL     	   0x0F
#define MCP_CANSTAT     	   0x0E
#define MCP_CANINTE     	   0x2B
#define MCP_CANINTF     	   0x2C
#define MCP_EFLG               0x2D
#define MCP_TXB0CTRL           0x30
#define MCP_TXB0SIDH           0x31
#define MCP_TXB0SIDL           0x32
#define MCP_TXB0EID8           0x33
#define MCP_TXB0EID0           0x34
#define MCP_TXB0DLC            0x35
#define MCP_TXB0D0             0x36

#define MCP_RXB0CTRL    	   0x60
#define MCP_RXB1CTRL    	   0x70
#define MCP_RXB0SIDH           0x61
#define MCP_RXB0SIDL           0x62
#define MCP_RXB0EID8           0x63
#define MCP_RXB0EID0           0x64
#define MCP_RXB0DLC            0x65
#define MCP_RXB0D0             0x66

/* Acceptance mask registers */
#define MCP_RXM0SIDH           0x20U
#define MCP_RXM0SIDL           0x21U
#define MCP_RXM0EID8           0x22U
#define MCP_RXM0EID0           0x23U

#define MCP_RXM1SIDH           0x24U
#define MCP_RXM1SIDL           0x25U
#define MCP_RXM1EID8           0x26U
#define MCP_RXM1EID0           0x27U

/* Acceptance filter registers */
#define MCP_RXF0SIDH           0x00U
#define MCP_RXF0SIDL           0x01U
#define MCP_RXF0EID8           0x02U
#define MCP_RXF0EID0           0x03U

#define MCP_RXF1SIDH           0x04U
#define MCP_RXF1SIDL           0x05U
#define MCP_RXF1EID8           0x06U
#define MCP_RXF1EID0           0x07U

#define MCP_RXF2SIDH           0x08U
#define MCP_RXF2SIDL           0x09U
#define MCP_RXF2EID8           0x0AU
#define MCP_RXF2EID0           0x0BU

#define MCP_RXF3SIDH           0x10U
#define MCP_RXF3SIDL           0x11U
#define MCP_RXF3EID8           0x12U
#define MCP_RXF3EID0           0x13U

#define MCP_RXF4SIDH           0x14U
#define MCP_RXF4SIDL           0x15U
#define MCP_RXF4EID8           0x16U
#define MCP_RXF4EID0           0x17U

#define MCP_RXF5SIDH           0x18U
#define MCP_RXF5SIDL           0x19U
#define MCP_RXF5EID8           0x1AU
#define MCP_RXF5EID0           0x1BU


#define MCP_CNF1	    0x2A
#define MCP_CNF2	    0x29
#define MCP_CNF3	    0x28

#define MCP2515_RXB_READ_LEN      14U

#define MCP2515_RXB_SIDH_IDX      1U
#define MCP2515_RXB_SIDL_IDX      2U
#define MCP2515_RXB_EID8_IDX      3U
#define MCP2515_RXB_EID0_IDX      4U
#define MCP2515_RXB_DLC_IDX       5U
#define MCP2515_RXB_DATA0_IDX     6U

#define MCP2515_TXB_LOAD_LEN_BASE   6U

#define MCP2515_SIDL_EXIDE     0x08U
#define MCP2515_DLC_RTR        0x40U
#define MCP2515_DLC_MASK       0x0FU

#define MCP2515_TXB_SIDH_IDX        1U
#define MCP2515_TXB_SIDL_IDX        2U
#define MCP2515_TXB_EID8_IDX        3U
#define MCP2515_TXB_EID0_IDX        4U
#define MCP2515_TXB_DLC_IDX         5U
#define MCP2515_TXB_DATA0_IDX       6U

/* RXBnCTRL bits */
#define MCP_RXB_ACCEPT_ALL     0x60
#define MCP_RXB_FILTERS_ENABLED 0x00U


#define MCP2515_MAX_DLC 8U
#define MCP2515_STANDARD_ID_MAX 0x7FFU
#define MCP2515_EXTENDED_ID_MAX 0x1FFFFFFFU

typedef enum {
    MCP_STATE_IDLE,
    MCP_STATE_TX_LOAD,
    MCP_STATE_TX_RTS,
    MCP_STATE_INT_READ_FLAGS,
    MCP_STATE_RX0_READ_BUF,
    MCP_STATE_RX1_READ_BUF,
    MCP_STATE_INT_CLEAR_FLAG,
    MCP_STATE_ERROR_READ_EFLG
} MCP_State_t;

/* Buffers to store SPI data */
static uint8_t spi_tx_buf[14];
static uint8_t spi_rx_buf[14];

static volatile MCP_State_t mcp_state = MCP_STATE_IDLE;
static volatile uint8_t rx_message_ready = 0;
static volatile uint8_t tx_message_done = 0;
static volatile uint8_t mcp_error_flags = 0;
static volatile uint8_t mcp_error_ready = 0U;
static can_frame_t latest_rx_msg;

/* Control CS */
static inline void CS_Select(void) {
	HAL_GPIO_WritePin(mcp_cs_port, mcp_cs_pin, GPIO_PIN_RESET);
}

static inline void CS_Deselect(void) {
	HAL_GPIO_WritePin(mcp_cs_port, mcp_cs_pin, GPIO_PIN_SET);
}

/* Blocking transmission to be implemented during the configuration phase */
static void MCP2515_SPI_Tx_Blocking(uint8_t *data, uint16_t len) {
    CS_Select();
    HAL_SPI_Transmit(mcp_hspi, data, len, HAL_MAX_DELAY);
    CS_Deselect();
}

static uint8_t MCP2515_Read_Register_Blocking(uint8_t addr)
{
    uint8_t tx[3] = { MCP_READ, addr, 0x00 };
    uint8_t rx[3] = { 0 };

    CS_Select();
    HAL_SPI_TransmitReceive(mcp_hspi, tx, rx, 3, HAL_MAX_DELAY);
    CS_Deselect();

    return rx[2];
}

static void MCP2515_Write_Register_Blocking(uint8_t addr, uint8_t value)
{
    uint8_t cmd[3] = { MCP_WRITE, addr, value };
    MCP2515_SPI_Tx_Blocking(cmd, 3);
}

static void MCP2515_Bit_Modify_Blocking(uint8_t addr, uint8_t mask, uint8_t value)
{
    uint8_t cmd[4] = { MCP_BITMOD, addr, mask, value };
    MCP2515_SPI_Tx_Blocking(cmd, 4);
}

static MCP2515_Status_t MCP2515_Set_Mode_Blocking(MCP2515_Mode_t mode)
{
    MCP2515_Bit_Modify_Blocking(MCP_CANCTRL, MCP_MODE_MASK, mode);

    HAL_Delay(1);

    uint8_t canstat = MCP2515_Read_Register_Blocking(MCP_CANSTAT);

    if ((canstat & MCP_MODE_MASK) != (mode & MCP_MODE_MASK)) {
        return MCP2515_ERROR_REQUESTED_MODE;
    }

    return MCP2515_OK;
}

MCP2515_Status_t MCP2515_Set_Mode(MCP2515_Mode_t mode)
{
    if ((mcp_hspi == NULL) || (mcp_cs_port == NULL)) {
        return MCP2515_ERROR_INVALID_ARG;
    }

    if (mcp_state != MCP_STATE_IDLE) {
        return MCP2515_ERROR_BUSY;
    }

    if (mcp_initialized == 0U) {
        return MCP2515_ERROR_NOT_READY;
    }

    return MCP2515_Set_Mode_Blocking(mode);
}

static MCP2515_Status_t MCP2515_Reset_Blocking(void)
{
    uint8_t cmd = MCP_RESET;

    CS_Deselect();
    HAL_Delay(1);

    CS_Select();

    if (HAL_SPI_Transmit(mcp_hspi, &cmd, 1U, HAL_MAX_DELAY) != HAL_OK) {
        CS_Deselect();
        return MCP2515_ERROR_SPI;
    }

    CS_Deselect();

    HAL_Delay(10);

    return MCP2515_OK;
}

static void MCP2515_Config_Accept_All_Blocking(void)
{
    MCP2515_Write_Register_Blocking(MCP_RXB0CTRL, MCP_RXB_ACCEPT_ALL);
    MCP2515_Write_Register_Blocking(MCP_RXB1CTRL, MCP_RXB_ACCEPT_ALL);
}

/* Initialization */
MCP2515_Status_t MCP2515_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, MCP2515_Mode_t mode) {
	mcp_hspi = hspi;
	mcp_cs_port = cs_port;
	mcp_cs_pin = cs_pin;
	mcp_state = MCP_STATE_IDLE;
	mcp_int_pending = 0U;
	mcp_int_flags = 0U;
	mcp_initialized = 0U;

	rx_message_ready = 0U;
	tx_message_done = 0U;

	mcp_error_flags = 0U;
	mcp_error_ready = 0U;

	/* Reset MCP2515 and expect Configuration Mode afterwards. */
	MCP2515_Status_t reset_status = MCP2515_Reset_Blocking();

	if (reset_status != MCP2515_OK) {
	    return reset_status;
	}

	uint8_t canstat = MCP2515_Read_Register_Blocking(MCP_CANSTAT);

	if ((canstat & MCP_MODE_MASK) != MCP2515_MODE_CONFIG) {
	// MCP2515 did not enter Configuration Mode.
	    return MCP2515_ERROR_CONFIG_MODE;
	}

	/* Configure CAN bit timing for 125 kbit/s with 8 MHz MCP2515 oscillator. */
	MCP2515_Write_Register_Blocking(MCP_CNF1, 0x01);	// CNF1
	MCP2515_Write_Register_Blocking(MCP_CNF2, 0xB1);	// CNF2
	MCP2515_Write_Register_Blocking(MCP_CNF3, 0x05);	// CNF3

	/* Start with accept-all filtering. Application may configure ID filters later. */
	MCP2515_Config_Accept_All_Blocking();

	/* Clear all pending interrupt flags before enabling interrupts. */
	MCP2515_Bit_Modify_Blocking(MCP_CANINTF, MCP_INT_ALL_FLAGS, 0x00);

	/* Enable RX, TX-complete, and error interrupts. */
	MCP2515_Write_Register_Blocking(MCP_CANINTE, MCP_INTE_RX0IE | MCP_INTE_RX1IE | MCP_INTE_TX0IE | MCP_INTE_ERRIE | MCP_INTE_MERRE);

	/* Switch to the requested operation mode. */
	MCP2515_Status_t mode_status = MCP2515_Set_Mode_Blocking(mode);

	if (mode_status != MCP2515_OK) {
	    return mode_status;
	}

	mcp_initialized = 1U;
    return MCP2515_OK;

}

static void MCP2515_Start_Read_INTF_IT(void)
{
    mcp_state = MCP_STATE_INT_READ_FLAGS;

    spi_tx_buf[0] = MCP_READ;
    spi_tx_buf[1] = MCP_CANINTF;
    spi_tx_buf[2] = 0x00;

    CS_Select();

    if (HAL_SPI_TransmitReceive_IT(mcp_hspi, spi_tx_buf, spi_rx_buf, 3) != HAL_OK) {
        CS_Deselect();
        mcp_state = MCP_STATE_IDLE;
        mcp_int_pending = 1;
    }
}

static uint32_t MCP2515_Decode_Standard_Id(uint8_t sidh, uint8_t sidl)
{
    return ((uint32_t)sidh << 3) | ((uint32_t)sidl >> 5);
}

static uint32_t MCP2515_Decode_Extended_Id(uint8_t sidh, uint8_t sidl, uint8_t eid8, uint8_t eid0)
{
    uint32_t id = 0U;

    id |= ((uint32_t)sidh << 21);
    id |= ((uint32_t)(sidl & 0xE0U) << 13);
    id |= ((uint32_t)(sidl & 0x03U) << 16);
    id |= ((uint32_t)eid8 << 8);
    id |= (uint32_t)eid0;

    return id;
}

static void MCP2515_Decode_RXB_Message(can_frame_t *frame)
{
    uint8_t sidh = spi_rx_buf[MCP2515_RXB_SIDH_IDX];
    uint8_t sidl = spi_rx_buf[MCP2515_RXB_SIDL_IDX];
    uint8_t eid8 = spi_rx_buf[MCP2515_RXB_EID8_IDX];
    uint8_t eid0 = spi_rx_buf[MCP2515_RXB_EID0_IDX];

    if (sidl & MCP2515_SIDL_EXIDE) {
        frame->id_type = CAN_ID_EXTENDED;
        frame->id = MCP2515_Decode_Extended_Id(sidh, sidl, eid8, eid0);
    } else {
        frame->id_type = CAN_ID_STANDARD;
        frame->id = MCP2515_Decode_Standard_Id(sidh, sidl);
    }

    uint8_t dlc_reg = spi_rx_buf[MCP2515_RXB_DLC_IDX];

    if (dlc_reg & MCP2515_DLC_RTR) {
        frame->frame_type = CAN_FRAME_REMOTE;
    } else {
        frame->frame_type = CAN_FRAME_DATA;
    }

    frame->dlc = dlc_reg & MCP2515_DLC_MASK;

    if (frame->dlc > MCP2515_MAX_DLC) {
    	frame->dlc = MCP2515_MAX_DLC;
    }

    for (uint8_t i = 0U; i < MCP2515_MAX_DLC; i++) {
        frame->data[i] = 0U;
    }

    if (frame->frame_type == CAN_FRAME_DATA) {
        for (uint8_t i = 0U; i < frame->dlc; i++) {
            frame->data[i] = spi_rx_buf[MCP2515_RXB_DATA0_IDX + i];
        }
    }
}

static void MCP2515_Encode_Standard_Id(uint32_t id, uint8_t *sidh, uint8_t *sidl, uint8_t *eid8, uint8_t *eid0)
{
    *sidh = (uint8_t)(id >> 3);
    *sidl = (uint8_t)((id & 0x07U) << 5);
    *eid8 = 0x00U;
    *eid0 = 0x00U;
}


static void MCP2515_Encode_Extended_Id(uint32_t id, uint8_t *sidh, uint8_t *sidl, uint8_t *eid8, uint8_t *eid0)
{
    *sidh = (uint8_t)((id >> 21) & 0xFFU);
    *sidl = (uint8_t)(((id >> 13) & 0xE0U) | MCP2515_SIDL_EXIDE | ((id >> 16) & 0x03U));
    *eid8 = (uint8_t)((id >> 8) & 0xFFU);
    *eid0 = (uint8_t)(id & 0xFFU);
}

static void MCP2515_Build_TXB0_Message(const can_frame_t *frame)
{
    spi_tx_buf[0] = MCP_LOAD_TXB0;
    if (frame->id_type == CAN_ID_EXTENDED) {
    	MCP2515_Encode_Extended_Id(frame->id, &spi_tx_buf[MCP2515_TXB_SIDH_IDX], &spi_tx_buf[MCP2515_TXB_SIDL_IDX], &spi_tx_buf[MCP2515_TXB_EID8_IDX], &spi_tx_buf[MCP2515_TXB_EID0_IDX]);
    } else {
        MCP2515_Encode_Standard_Id(frame->id, &spi_tx_buf[MCP2515_TXB_SIDH_IDX], &spi_tx_buf[MCP2515_TXB_SIDL_IDX], &spi_tx_buf[MCP2515_TXB_EID8_IDX], &spi_tx_buf[MCP2515_TXB_EID0_IDX]);
    }

    spi_tx_buf[MCP2515_TXB_DLC_IDX] = frame->dlc & MCP2515_DLC_MASK;

    if (frame->frame_type == CAN_FRAME_REMOTE) {
        spi_tx_buf[MCP2515_TXB_DLC_IDX] |= MCP2515_DLC_RTR;
    } else {
        for (uint8_t i = 0U; i < frame->dlc; i++) {
            spi_tx_buf[MCP2515_TXB_DATA0_IDX + i] = frame->data[i];
        }
    }
}

static void MCP2515_Write_Id_Blocking(uint8_t sidh_addr, uint32_t id, can_id_type_t id_type)
{
    uint8_t sidh = 0U;
    uint8_t sidl = 0U;
    uint8_t eid8 = 0U;
    uint8_t eid0 = 0U;

    if (id_type == CAN_ID_EXTENDED) {
        MCP2515_Encode_Extended_Id(id, &sidh, &sidl, &eid8, &eid0);
    } else {
        MCP2515_Encode_Standard_Id(id, &sidh, &sidl, &eid8, &eid0);
    }

    MCP2515_Write_Register_Blocking(sidh_addr, sidh);
    MCP2515_Write_Register_Blocking(sidh_addr + 1U, sidl);
    MCP2515_Write_Register_Blocking(sidh_addr + 2U, eid8);
    MCP2515_Write_Register_Blocking(sidh_addr + 3U, eid0);
}


MCP2515_Status_t MCP2515_Config_Accept_All(MCP2515_Mode_t restore_mode)
{
    if (mcp_initialized == 0U) {
        return MCP2515_ERROR_NOT_READY;
    }

    if (mcp_state != MCP_STATE_IDLE) {
        return MCP2515_ERROR_BUSY;
    }

    MCP2515_Status_t status = MCP2515_Set_Mode_Blocking(MCP2515_MODE_CONFIG);

    if (status != MCP2515_OK) {
        return status;
    }

    MCP2515_Config_Accept_All_Blocking();

    status = MCP2515_Set_Mode_Blocking(restore_mode);

    if (status != MCP2515_OK) {
        return status;
    }

    return MCP2515_OK;
}

MCP2515_Status_t MCP2515_Config_Id_Filter(MCP2515_Mode_t restore_mode, uint32_t filter_id, uint32_t mask_id, can_id_type_t id_type)
{
    if (mcp_initialized == 0U) {
        return MCP2515_ERROR_NOT_READY;
    }

    if (mcp_state != MCP_STATE_IDLE) {
        return MCP2515_ERROR_BUSY;
    }

    if (id_type == CAN_ID_STANDARD) {
        if ((filter_id > MCP2515_STANDARD_ID_MAX) ||
            (mask_id > MCP2515_STANDARD_ID_MAX)) {
            return MCP2515_ERROR_INVALID_ARG;
        }
    } else if (id_type == CAN_ID_EXTENDED) {
        if ((filter_id > MCP2515_EXTENDED_ID_MAX) ||
            (mask_id > MCP2515_EXTENDED_ID_MAX)) {
            return MCP2515_ERROR_INVALID_ARG;
        }
    } else {
        return MCP2515_ERROR_INVALID_ARG;
    }

    MCP2515_Status_t status = MCP2515_Set_Mode_Blocking(MCP2515_MODE_CONFIG);

    if (status != MCP2515_OK) {
        return status;
    }

    MCP2515_Write_Id_Blocking(MCP_RXM0SIDH, mask_id, id_type);
    MCP2515_Write_Id_Blocking(MCP_RXF0SIDH, filter_id, id_type);
    MCP2515_Write_Id_Blocking(MCP_RXF1SIDH, filter_id, id_type);
    /* Apply the same rule to RXB1 as well, to avoid RXB1 accepting rejected RXB0 frames. */
    MCP2515_Write_Id_Blocking(MCP_RXM1SIDH, mask_id, id_type);
    MCP2515_Write_Id_Blocking(MCP_RXF2SIDH, filter_id, id_type);
    MCP2515_Write_Id_Blocking(MCP_RXF3SIDH, filter_id, id_type);
    MCP2515_Write_Id_Blocking(MCP_RXF4SIDH, filter_id, id_type);
    MCP2515_Write_Id_Blocking(MCP_RXF5SIDH, filter_id, id_type);

    /* Enable mask/filter acceptance on RXB0. */
    MCP2515_Write_Register_Blocking(MCP_RXB0CTRL, MCP_RXB_FILTERS_ENABLED);
    MCP2515_Write_Register_Blocking(MCP_RXB1CTRL, MCP_RXB_FILTERS_ENABLED);

    status = MCP2515_Set_Mode_Blocking(restore_mode);

    if (status != MCP2515_OK) {
        return status;
    }

    return MCP2515_OK;
}


static MCP2515_Status_t MCP2515_Validate_Frame(const can_frame_t *frame)
{
    if (frame == NULL) {
        return MCP2515_ERROR_INVALID_ARG;
    }

    if (frame->dlc > MCP2515_MAX_DLC) {
        return MCP2515_ERROR_INVALID_ARG;
    }

    if ((frame->frame_type != CAN_FRAME_DATA) && (frame->frame_type != CAN_FRAME_REMOTE)) {
        return MCP2515_ERROR_INVALID_ARG;
    }

    if (frame->id_type == CAN_ID_STANDARD) {
            if (frame->id > MCP2515_STANDARD_ID_MAX) {
                return MCP2515_ERROR_INVALID_ARG;
            }
    } else if (frame->id_type == CAN_ID_EXTENDED) {
            if (frame->id > MCP2515_EXTENDED_ID_MAX) {
                return MCP2515_ERROR_INVALID_ARG;
            }
    } else {
            return MCP2515_ERROR_INVALID_ARG;
    }

    return MCP2515_OK;
}

/* Frame Transmission Via SPI (Non-blocking */
MCP2515_Status_t MCP2515_Send_Frame_IT(const can_frame_t *frame) {

	MCP2515_Status_t status = MCP2515_Validate_Frame(frame);

	if (status != MCP2515_OK) {
	    return status;
	}

	if (mcp_initialized == 0U) {
	    return MCP2515_ERROR_NOT_READY;
	}

	if (mcp_state!= MCP_STATE_IDLE){
		return MCP2515_ERROR_BUSY;
	}

    mcp_state = MCP_STATE_TX_LOAD;

    MCP2515_Build_TXB0_Message(frame);

    CS_Select();
    uint16_t tx_len = MCP2515_TXB_LOAD_LEN_BASE;

    if (frame->frame_type == CAN_FRAME_DATA) {
        tx_len += frame->dlc;
    }

    if (HAL_SPI_TransmitReceive_IT(mcp_hspi, spi_tx_buf, spi_rx_buf, tx_len) != HAL_OK) {
        CS_Deselect();
        mcp_state = MCP_STATE_IDLE;
        return MCP2515_ERROR_SPI;
    }

    return MCP2515_OK;
}

/* Handler for EXTI */
void MCP2515_EXTI_Handler(void) {

	 mcp_int_pending = 1;

    if (mcp_state == MCP_STATE_IDLE) {
        mcp_int_pending = 0;
        MCP2515_Start_Read_INTF_IT();
    }
}

static void MCP2515_Service_Pending_INT(void)
{
    if ((mcp_state == MCP_STATE_IDLE) && (mcp_int_pending != 0)) {
        mcp_int_pending = 0;
        MCP2515_Start_Read_INTF_IT();
    }
}

static void MCP2515_Start_Clear_INTF_IT(uint8_t intf_mask)
{
    mcp_state = MCP_STATE_INT_CLEAR_FLAG;

    spi_tx_buf[0] = MCP_BITMOD;
    spi_tx_buf[1] = MCP_CANINTF;
    spi_tx_buf[2] = intf_mask;
    spi_tx_buf[3] = 0x00;

    CS_Select();

    if (HAL_SPI_TransmitReceive_IT(mcp_hspi, spi_tx_buf, spi_rx_buf, 4U) != HAL_OK) {
        CS_Deselect();
        mcp_state = MCP_STATE_IDLE;
    }
}

static void MCP2515_Start_Read_RXB_IT(uint8_t read_command, MCP_State_t next_state)
{
    mcp_state = next_state;

    spi_tx_buf[0] = read_command;

    for (uint8_t i = 1U; i < MCP2515_RXB_READ_LEN; i++) {
        spi_tx_buf[i] = 0x00;
    }

    CS_Select();

    if (HAL_SPI_TransmitReceive_IT(mcp_hspi,
                                   spi_tx_buf,
                                   spi_rx_buf,
                                   MCP2515_RXB_READ_LEN) != HAL_OK) {
        CS_Deselect();
        mcp_state = MCP_STATE_IDLE;
    }
}

static void MCP2515_Start_Read_EFLG_IT(void)
{
    mcp_state = MCP_STATE_ERROR_READ_EFLG;

    spi_tx_buf[0] = MCP_READ;
    spi_tx_buf[1] = MCP_EFLG;
    spi_tx_buf[2] = 0x00;

    CS_Select();

    if (HAL_SPI_TransmitReceive_IT(mcp_hspi, spi_tx_buf, spi_rx_buf, 3U) != HAL_OK) {
        CS_Deselect();
        mcp_state = MCP_STATE_IDLE;
    }
}

/* Callback for SPI Interrupt */
void MCP2515_SPI_TxRxCplt_Handler(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance!= mcp_hspi->Instance) return;

    /* Close any previous transmission */
    CS_Deselect();

    switch (mcp_state) {
		case MCP_STATE_TX_LOAD:
			mcp_state = MCP_STATE_TX_RTS;
			spi_tx_buf[0] = MCP_RTS_TX0;

			CS_Select();
			if(HAL_SPI_TransmitReceive_IT(mcp_hspi, spi_tx_buf, spi_rx_buf, 1) != HAL_OK) {
	            CS_Deselect();
	            mcp_state = MCP_STATE_IDLE;
			}
			break;
		case MCP_STATE_TX_RTS:
			mcp_state = MCP_STATE_IDLE;
			MCP2515_Service_Pending_INT();
			break;

		case MCP_STATE_INT_READ_FLAGS:
			mcp_int_flags = spi_rx_buf[2];

			if (mcp_int_flags & MCP_INT_RX0IF) {
			    MCP2515_Start_Read_RXB_IT(MCP_READ_RXB0, MCP_STATE_RX0_READ_BUF);
			}
			else if (mcp_int_flags & MCP_INT_RX1IF) {
			    MCP2515_Start_Read_RXB_IT(MCP_READ_RXB1, MCP_STATE_RX1_READ_BUF);
			}
		    else if (mcp_int_flags & MCP_INT_TX0IF) {
		        tx_message_done = 1U;
		        MCP2515_Start_Clear_INTF_IT(MCP_INT_TX0IF);
		    }
		    else if (mcp_int_flags & (MCP_INT_ERRIF | MCP_INT_MERRF)) {
		        MCP2515_Start_Read_EFLG_IT();
		    }
			else {
				mcp_state = MCP_STATE_IDLE;
				MCP2515_Service_Pending_INT();
			}
			break;

		case MCP_STATE_RX0_READ_BUF:
			MCP2515_Decode_RXB_Message(&latest_rx_msg);
			rx_message_ready = 1U;

			if (mcp_int_flags & (uint8_t)(~MCP_INT_RX0IF)) {
			    mcp_int_pending = 1U;
			}
			MCP2515_Start_Clear_INTF_IT(MCP_INT_RX0IF);

		    break;
		case MCP_STATE_RX1_READ_BUF:
		    MCP2515_Decode_RXB_Message(&latest_rx_msg);
		    rx_message_ready = 1U;

		    if (mcp_int_flags & (uint8_t)(~MCP_INT_RX1IF)) {
		        mcp_int_pending = 1U;
		    }

		    MCP2515_Start_Clear_INTF_IT(MCP_INT_RX1IF);
		    break;
		case MCP_STATE_INT_CLEAR_FLAG:
			mcp_state = MCP_STATE_IDLE;
			MCP2515_Service_Pending_INT();
			break;
		case MCP_STATE_ERROR_READ_EFLG:
		    mcp_error_flags = spi_rx_buf[2];
		    mcp_error_ready = 1U;
		    /*
		     * For now, only clear MERRF if it was set.
		     * ERRIF may remain active while EFLG still reports an error condition.
		     */
		    if (mcp_int_flags & MCP_INT_MERRF) {
		        MCP2515_Start_Clear_INTF_IT(MCP_INT_MERRF);
		    } else {
		        mcp_state = MCP_STATE_IDLE;
		        MCP2515_Service_Pending_INT();
		    }
		    break;
		default:
			mcp_state = MCP_STATE_IDLE;
			break;
    }

}

/**
 * @brief Read and clear the latest received CAN frame if available.
 *
 * The driver stores only one received frame internally. If another frame arrives
 * before the application reads the previous one, the previous frame may be
 * overwritten.
 */
uint8_t MCP2515_Read_Frame(can_frame_t *out_frame)
{
    if (out_frame == NULL) {
        return 0;
    }

    if (rx_message_ready == 0) {
        return 0;
    }

    __disable_irq();
    *out_frame = latest_rx_msg;
    rx_message_ready = 0;
    __enable_irq();
    return 1;
}

uint8_t MCP2515_Take_Tx_Done(void)
{
    uint8_t done = 0;

    __disable_irq();

    if (tx_message_done != 0U) {
        tx_message_done = 0U;
        done = 1U;
    }

    __enable_irq();

    return done;
}

uint8_t MCP2515_Take_Error_Flags(uint8_t *out_error_flags)
{
    if (out_error_flags == NULL) {
        return 0U;
    }

    if (mcp_error_ready == 0U) {
        return 0U;
    }

    __disable_irq();
    *out_error_flags = mcp_error_flags;
    mcp_error_ready = 0U;
    __enable_irq();

    return 1U;
}

uint8_t MCP2515_Is_Busy(void)
{
    return (mcp_state != MCP_STATE_IDLE) ? 1U : 0U;
}


static can_if_status_t MCP2515_Map_Status(MCP2515_Status_t status)
{
    switch (status) {
        case MCP2515_OK:
            return CAN_IF_OK;

        case MCP2515_ERROR_INVALID_ARG:
            return CAN_IF_ERROR_INVALID_ARG;

        case MCP2515_ERROR_NOT_READY:
            return CAN_IF_ERROR_NOT_READY;

        case MCP2515_ERROR_BUSY:
            return CAN_IF_ERROR_BUSY;

        case MCP2515_ERROR_CONFIG_MODE:
        case MCP2515_ERROR_REQUESTED_MODE:
        case MCP2515_ERROR_SPI:
        default:
            return CAN_IF_ERROR_BACKEND;
    }
}

static can_if_status_t MCP2515_CAN_IF_Send(void *context, const can_frame_t *frame)
{
    (void)context;

    return MCP2515_Map_Status(MCP2515_Send_Frame_IT(frame));
}

static uint8_t MCP2515_CAN_IF_Read(void *context, can_frame_t *out_frame)
{
    (void)context;

    return MCP2515_Read_Frame(out_frame);
}

static uint8_t MCP2515_CAN_IF_Tx_Done(void *context)
{
    (void)context;

    return MCP2515_Take_Tx_Done();
}

static uint8_t MCP2515_CAN_IF_Is_Busy(void *context)
{
    (void)context;

    return MCP2515_Is_Busy();
}

MCP2515_Status_t MCP2515_Get_CAN_IF_Backend(can_if_backend_t *backend)
{
    if (backend == NULL) {
        return MCP2515_ERROR_INVALID_ARG;
    }

    backend->context = NULL;
    backend->send = MCP2515_CAN_IF_Send;
    backend->read = MCP2515_CAN_IF_Read;
    backend->tx_done = MCP2515_CAN_IF_Tx_Done;
    backend->is_busy = MCP2515_CAN_IF_Is_Busy;

    return MCP2515_OK;
}
