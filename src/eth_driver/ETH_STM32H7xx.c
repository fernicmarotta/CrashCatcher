/**
 * @file ETH_STM32H7xx.c
 * @brief CMSIS-Driver compliant Ethernet MAC driver for STM32H7xx
 */

#include "Driver_ETH_MAC.h"
#include "Driver_ETH_PHY.h"
#include "stm32h7xx_hal.h"
#include "PHY_DP83848C.h"
#include <string.h>

/* ETH HAL handle */
ETH_HandleTypeDef heth;
ETH_TxPacketConfig TxConfig;

/* Driver version */
#define ARM_ETH_MAC_DRV_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)

/* Driver capabilities */
static const ARM_ETH_MAC_CAPABILITIES DriverCapabilities = {
    0U,  /* checksum_offload_rx_ip4  */
    0U,  /* checksum_offload_rx_ip6  */
    0U,  /* checksum_offload_rx_udp  */
    0U,  /* checksum_offload_rx_tcp  */
    0U,  /* checksum_offload_rx_icmp */
    0U,  /* checksum_offload_tx_ip4  */
    0U,  /* checksum_offload_tx_ip6  */
    0U,  /* checksum_offload_tx_udp  */
    0U,  /* checksum_offload_tx_tcp  */
    0U,  /* checksum_offload_tx_icmp */
    0U,  /* media_interface          */
    0U,  /* mac_address              */
    0U,  /* event_rx_frame           */
    0U,  /* event_tx_frame           */
    0U,  /* event_wakeup             */
    0U,  /* precision_timer          */
    0U   /* reserved                 */
};

/* Driver Version */
static const ARM_DRIVER_VERSION DriverVersion = {
    ARM_ETH_MAC_API_VERSION,
    ARM_ETH_MAC_DRV_VERSION
};

/* Ethernet MAC Driver Control Block */
static struct {
    ARM_ETH_MAC_SignalEvent_t cb_event;
    uint8_t                   flags;
    uint8_t                   mac_address[6];
} ETH_MAC;

/**
  * Get driver version.
  */
static ARM_DRIVER_VERSION GetVersion(void) {
    return DriverVersion;
}

/**
  * Get driver capabilities.
  */
static ARM_ETH_MAC_CAPABILITIES GetCapabilities(void) {
    return DriverCapabilities;
}

/**
  * Initialize Ethernet MAC Device.
  */
static int32_t Initialize(ARM_ETH_MAC_SignalEvent_t cb_event) {
    ETH_MAC.cb_event = cb_event;
    
    /* Configure Ethernet GPIO */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    /* RMII pins configuration */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    
    /* Configure ETH pins */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* Enable Ethernet clocks */
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
    
    /* Reset Ethernet */
    __HAL_RCC_ETH1MAC_FORCE_RESET();
    __HAL_RCC_ETH1MAC_RELEASE_RESET();
    
    return ARM_DRIVER_OK;
}

/**
  * De-initialize Ethernet MAC Device.
  */
static int32_t Uninitialize(void) {
    /* Disable Ethernet clocks */
    __HAL_RCC_ETH1MAC_CLK_DISABLE();
    __HAL_RCC_ETH1TX_CLK_DISABLE();
    __HAL_RCC_ETH1RX_CLK_DISABLE();
    
    return ARM_DRIVER_OK;
}

/**
  * Control Ethernet MAC Device Power.
  */
static int32_t PowerControl(ARM_POWER_STATE state) {
    switch (state) {
        case ARM_POWER_OFF:
            HAL_ETH_Stop(&heth);
            break;
            
        case ARM_POWER_LOW:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
            
        case ARM_POWER_FULL:
            /* Initialize ETH peripheral */
            heth.Instance = ETH;
            heth.Init.MACAddr = ETH_MAC.mac_address;
            heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
            heth.Init.RxDesc = NULL;
            heth.Init.TxDesc = NULL;
            heth.Init.RxBuffLen = ETH_RX_BUFFER_SIZE;
            
            if (HAL_ETH_Init(&heth) != HAL_OK) {
                return ARM_DRIVER_ERROR;
            }
            
            /* Initialize Tx Packet config */
            memset(&TxConfig, 0, sizeof(ETH_TxPacketConfig));
            TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
            TxConfig.ChecksumCtrl = ETH_CHECKSUM_DISABLE;
            TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
            
            break;
            
        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
    
    return ARM_DRIVER_OK;
}

/**
  * Get Ethernet MAC Address.
  */
static int32_t GetMacAddress(ARM_ETH_MAC_ADDR *ptr_addr) {
    if (ptr_addr == NULL) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    
    memcpy(ptr_addr, ETH_MAC.mac_address, 6);
    return ARM_DRIVER_OK;
}

/**
  * Set Ethernet MAC Address.
  */
static int32_t SetMacAddress(const ARM_ETH_MAC_ADDR *ptr_addr) {
    if (ptr_addr == NULL) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    
    memcpy(ETH_MAC.mac_address, ptr_addr, 6);
    
    /* Update HAL ETH */
    if (heth.Instance != NULL) {
        HAL_ETH_SetMACAddress(&heth, (uint8_t *)ptr_addr);
    }
    
    return ARM_DRIVER_OK;
}

/**
  * Configure Address Filter.
  */
static int32_t SetAddressFilter(const ARM_ETH_MAC_ADDR *ptr_addr, uint32_t num_addr) {
    /* Not implemented in this minimal version */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  * Send Ethernet frame.
  */
static int32_t SendFrame(const uint8_t *frame, uint32_t len, uint32_t flags) {
    if ((frame == NULL) || (len == 0U)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    
    /* Prepare Tx buffer */
    ETH_BufferTypeDef Txbuffer[1];
    Txbuffer[0].buffer = (uint8_t *)frame;
    Txbuffer[0].len = len;
    Txbuffer[0].next = NULL;
    
    TxConfig.Length = len;
    TxConfig.TxBuffer = Txbuffer;
    
    /* Transmit frame */
    if (HAL_ETH_Transmit(&heth, &TxConfig, 1000) != HAL_OK) {
        return ARM_DRIVER_ERROR;
    }
    
    return ARM_DRIVER_OK;
}

/**
  * Read data of received Ethernet frame.
  */
static int32_t ReadFrame(uint8_t *frame, uint32_t len) {
    if ((frame == NULL) || (len == 0U)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    
    ETH_BufferTypeDef RxBuff;
    uint32_t frameLength = 0;
    
    if (HAL_ETH_GetRxDataBuffer(&heth, &RxBuff) == HAL_OK) {
        /* Copy received data */
        frameLength = RxBuff.len;
        if (frameLength > len) {
            frameLength = len;
        }
        
        memcpy(frame, RxBuff.buffer, frameLength);
        
        /* Release buffer */
        HAL_ETH_BuildRxDescriptors(&heth);
        
        return (int32_t)frameLength;
    }
    
    return 0;
}

/**
  * Get size of received Ethernet frame.
  */
static uint32_t GetRxFrameSize(void) {
    ETH_BufferTypeDef RxBuff;
    
    if (HAL_ETH_IsRxDataAvailable(&heth)) {
        HAL_ETH_GetRxDataBuffer(&heth, &RxBuff);
        return RxBuff.len;
    }
    
    return 0U;
}

/**
  * Get time of received Ethernet frame.
  */
static int32_t GetRxFrameTime(ARM_ETH_MAC_TIME *time) {
    /* Not implemented */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  * Get transmit frame time.
  */
static int32_t GetTxFrameTime(ARM_ETH_MAC_TIME *time) {
    /* Not implemented */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  * Control Ethernet Interface.
  */
static int32_t ControlTimer(uint32_t control, ARM_ETH_MAC_TIME *time) {
    /* Not implemented */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  * Control Ethernet Interface.
  */
static int32_t Control(uint32_t control, uint32_t arg) {
    switch (control) {
        case ARM_ETH_MAC_CONFIGURE:
            /* Configure MAC */
            if (arg & ARM_ETH_MAC_SPEED_100M) {
                /* 100 Mbps */
                heth.Init.Speed = ETH_SPEED_100M;
            } else {
                /* 10 Mbps */
                heth.Init.Speed = ETH_SPEED_10M;
            }
            
            if (arg & ARM_ETH_MAC_DUPLEX_FULL) {
                /* Full duplex */
                heth.Init.DuplexMode = ETH_FULLDUPLEX_MODE;
            } else {
                /* Half duplex */
                heth.Init.DuplexMode = ETH_HALFDUPLEX_MODE;
            }
            
            HAL_ETH_SetMACConfig(&heth);
            break;
            
        case ARM_ETH_MAC_CONTROL_TX:
            /* Enable/disable transmitter */
            if (arg != 0U) {
                HAL_ETH_Start(&heth);
            } else {
                HAL_ETH_Stop(&heth);
            }
            break;
            
        case ARM_ETH_MAC_CONTROL_RX:
            /* Enable/disable receiver */
            if (arg != 0U) {
                HAL_ETH_Start(&heth);
            } else {
                HAL_ETH_Stop(&heth);
            }
            break;
            
        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
    
    return ARM_DRIVER_OK;
}

/**
  * Read Ethernet PHY Register through Management Interface.
  */
static int32_t PHY_Read(uint8_t phy_addr, uint8_t reg_addr, uint16_t *data) {
    uint32_t temp;
    
    if (HAL_ETH_ReadPHYRegister(&heth, phy_addr, reg_addr, &temp) != HAL_OK) {
        return ARM_DRIVER_ERROR;
    }
    
    *data = (uint16_t)temp;
    return ARM_DRIVER_OK;
}

/**
  * Write Ethernet PHY Register through Management Interface.
  */
static int32_t PHY_Write(uint8_t phy_addr, uint8_t reg_addr, uint16_t data) {
    if (HAL_ETH_WritePHYRegister(&heth, phy_addr, reg_addr, data) != HAL_OK) {
        return ARM_DRIVER_ERROR;
    }
    
    return ARM_DRIVER_OK;
}

/* Ethernet MAC Driver Control Block */
ARM_DRIVER_ETH_MAC Driver_ETH_MAC0 = {
    GetVersion,
    GetCapabilities,
    Initialize,
    Uninitialize,
    PowerControl,
    GetMacAddress,
    SetMacAddress,
    SetAddressFilter,
    SendFrame,
    ReadFrame,
    GetRxFrameSize,
    GetRxFrameTime,
    GetTxFrameTime,
    ControlTimer,
    Control,
    PHY_Read,
    PHY_Write
};

/* Provide PHY driver */
extern ARM_DRIVER_ETH_PHY Driver_ETH_PHY0;