/* Copyright (c) Nordic Semiconductor ASA
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 * 
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of other
 *   contributors to this software may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 * 
 *   4. This software must only be used in a processor manufactured by Nordic
 *   Semiconductor ASA, or in a processor manufactured by a third party that
 *   is used in combination with a processor manufactured by Nordic Semiconductor.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "hal_radio.h"
#include "hal_timer.h"
#include "hal_clock.h"
#include "drv_lps25h.h"
#include "drv_lps25h_bitfields.h"
#include "hal_serial.h"
#include "hal_twi.h"

#include "nrf.h"
#include "ble_pdlp_beacon.h"
#include "ble_pdlp_common.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


//#define DBG_RADIO_ACTIVE_ENABLE

//#define DBG_BEACON_START_PIN    (16)
//#define DBG_HFCLK_ENABLED_PIN   (27)
//#define DBG_HFCLK_DISABLED_PIN  (26)
//#define DBG_PKT_SENT_PIN        (9)
//#define DBG_CALCULATE_BEGIN_PIN (15)
//#define DBG_CALCULATE_END_PIN   (15)
//#define DBG_RADIO_ACTIVE_PIN    (24)
//#define DBG_WFE_BEGIN_PIN (25)
//#define DBG_WFE_END_PIN   (25)


//#define DBG_BEACON_START                              \
//{                                                     \
//    NRF_GPIO->OUTSET = (1 << DBG_BEACON_START_PIN);   \
//    NRF_GPIO->DIRSET = (1 << DBG_BEACON_START_PIN);   \
//    NRF_GPIO->DIRCLR = (1 << DBG_BEACON_START_PIN);   \
//    NRF_GPIO->OUTCLR = (1 << DBG_BEACON_START_PIN);   \
//}                                                     
//#define DBG_HFCLK_ENABLED                             \
//{                                                     \
//    NRF_GPIO->OUTSET = (1 << DBG_HFCLK_ENABLED_PIN);  \
//    NRF_GPIO->DIRSET = (1 << DBG_HFCLK_ENABLED_PIN);  \
//    NRF_GPIO->DIRCLR = (1 << DBG_HFCLK_ENABLED_PIN);  \
//    NRF_GPIO->OUTCLR = (1 << DBG_HFCLK_ENABLED_PIN);  \
//}                                                     
//#define DBG_HFCLK_DISABLED                            \
//{                                                     \
//    NRF_GPIO->OUTSET = (1 << DBG_HFCLK_DISABLED_PIN); \
//    NRF_GPIO->DIRSET = (1 << DBG_HFCLK_DISABLED_PIN); \
//    NRF_GPIO->DIRCLR = (1 << DBG_HFCLK_DISABLED_PIN); \
//    NRF_GPIO->OUTCLR = (1 << DBG_HFCLK_DISABLED_PIN); \
//}                                                     
//#define DBG_PKT_SENT                                   \
//{                                                      \
//    NRF_GPIO->OUTSET = (1 << DBG_PKT_SENT_PIN);        \
//    NRF_GPIO->DIRSET = (1 << DBG_PKT_SENT_PIN);        \
//    NRF_GPIO->DIRCLR = (1 << DBG_PKT_SENT_PIN);        \
//    NRF_GPIO->OUTCLR = (1 << DBG_PKT_SENT_PIN);        \
//}                                                      
//#define DBG_WFE_BEGIN                            \
//{                                                \
//    NRF_GPIO->OUTCLR = (1 << DBG_WFE_BEGIN_PIN); \
//    NRF_GPIO->DIRSET = (1 << DBG_WFE_BEGIN_PIN); \
//    NRF_GPIO->OUTSET = (1 << DBG_WFE_BEGIN_PIN); \
//    for ( int i = 0; i < 0xFF; i++ ) __NOP();\
//    NRF_GPIO->OUTCLR = (1 << DBG_WFE_BEGIN_PIN); \
//}
//#define DBG_WFE_END                              \
//{                                                \
//    for ( int i = 0; i < 0xFF; i++ ) __NOP();\
//    NRF_GPIO->OUTSET = (1 << DBG_WFE_END_PIN);   \
//    /*NRF_GPIO->DIRSET = (1 << DBG_WFE_END_PIN);*/   \
//}


#ifndef DBG_BEACON_START
#define DBG_BEACON_START
#endif
#ifndef DBG_PKT_SENT
#define DBG_PKT_SENT
#endif
#ifndef DBG_HFCLK_ENABLED
#define DBG_HFCLK_ENABLED
#endif
#ifndef DBG_HFCLK_DISABLED
#define DBG_HFCLK_DISABLED
#endif
#ifndef DBG_WFE_BEGIN
#define DBG_WFE_BEGIN
#endif
#ifndef DBG_WFE_END
#define DBG_WFE_END
#endif


#ifdef PCA20014
static const hal_serial_cfg_t serial_cfg =
{
    .twi0.psel.scl = 27,
    .twi0.psel.sda = 26,
};    
#else
#ifdef PCA10036
static const hal_serial_cfg_t serial_cfg =
{
    .twi0.psel.scl = 25,
    .twi0.psel.sda = 23,
};
#else
static const hal_serial_cfg_t serial_cfg =
{
    .twi0.psel.scl = 29,
    .twi0.psel.sda = 25,    
};
#endif  
#endif


static uint8_t M_BEACON_PDU_TYPE = LINKING_SERVICE_TYPE_TEMPERATURE;

#define HFCLK_STARTUP_TIME_US                       (1600)              /* The time in microseconds it takes to start up the HF clock*. */
#define INTERVAL_US                                 (1000000)           /* The time in microseconds between advertising events. */
#define INITIAL_TIMEOUT                             (INTERVAL_US)       /* The time in microseconds until adverising the first time. */
#define START_OF_INTERVAL_TO_SENSOR_READ_TIME_US    (INTERVAL_US / 2)   /* The time from the start of the latest advertising event until reading the sensor. */
#define SENSOR_SKIP_READ_COUNT                      (10)                /* The number of advertising events between reading the sensor. */


#if INITIAL_TIMEOUT - HFCLK_STARTUP_TIME_US < 400
#error "Initial timeout too short!"
#endif


#define BD_ADDR_OFFS                (3)     /* BLE device address offest of the beacon advertising pdu. */
#define M_BD_ADDR_SIZE              (6)     /* BLE device address size. */

/* Begin - Definitions for beacons with both temperature and pressure. */
#define SINT16_TEMPERATURE_OFFS     (20)    /* The offset of the temperature in the beacon advertising pdu */
#define UINT32_PRESSURE_OFFS        (26)    /* The offset of the pressure in the beacon advertising pdu */
/* End - Definitions for beacons with both temperature and pressure. */

/* Begin - Definitions for beacons with only temperature. */
#define FLOAT32_TEMPERATURE_OFFS    (36)    /* The offset of the temperature in the beacon advertising pdu */
/* End - Definitions for beacons with only temperature. */


/* The beacon types.
 */
typedef enum
{
    M_BEACON_PDU_TYPE_TEMP_ONLY = 0,    ///< Beacon with only temperature data.
    M_BEACON_PDU_TYPE_TEMP_PRES,        ///< Beacon with both temperature and pressure data.
} m_beacon_pdu_type_t;


static bool volatile m_radio_isr_called;    /* Indicates that the radio ISR has executed. */
static bool volatile m_rtc_isr_called;      /* Indicates that the RTC ISR has executed. */
static uint32_t m_time_us;                  /* Keeps track of the latest scheduled point in time. */
static uint32_t m_skip_read_counter = 0;    /* Keeps track on when to read the sensor. */
static uint8_t m_adv_pdu[40];               /* The RAM representation of the advertising PDU. */


/* Initializes the beacon advertising PDU.
 */
static void m_beacon_pdu_init(uint8_t * p_beacon_pdu)
{
    p_beacon_pdu[0] = 0x42;
    p_beacon_pdu[1] = 0;
    p_beacon_pdu[2] = 0;
}


/* Sets the BLE address field in the sensor beacon PDU.
 */
static void m_beacon_pdu_bd_addr_default_set(uint8_t * p_beacon_pdu)
{
    if ( ( NRF_FICR->DEVICEADDR[0]           != 0xFFFFFFFF)
    ||   ((NRF_FICR->DEVICEADDR[1] & 0xFFFF) != 0xFFFF) )
    {
        p_beacon_pdu[BD_ADDR_OFFS    ] = (NRF_FICR->DEVICEADDR[0]      ) & 0xFF;
        p_beacon_pdu[BD_ADDR_OFFS + 1] = (NRF_FICR->DEVICEADDR[0] >>  8) & 0xFF;
        p_beacon_pdu[BD_ADDR_OFFS + 2] = (NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF;
        p_beacon_pdu[BD_ADDR_OFFS + 3] = (NRF_FICR->DEVICEADDR[0] >> 24)       ;
        p_beacon_pdu[BD_ADDR_OFFS + 4] = (NRF_FICR->DEVICEADDR[1]      ) & 0xFF;
        p_beacon_pdu[BD_ADDR_OFFS + 5] = (NRF_FICR->DEVICEADDR[1] >>  8) & 0xFF;
    }
    else
    {
        static const uint8_t random_bd_addr[M_BD_ADDR_SIZE] = {0xE2, 0xA3, 0x01, 0xE7, 0x61, 0xF7};
        memcpy(&(p_beacon_pdu[3]), &(random_bd_addr[0]), M_BD_ADDR_SIZE);
    }
    
    p_beacon_pdu[1] = (p_beacon_pdu[1] == 0) ? M_BD_ADDR_SIZE : p_beacon_pdu[1];
}


/* Resets the sensor data of the sensor beacon PDU.
 */
static void m_beacon_pdu_sensor_data_reset(uint8_t * p_beacon_pdu)
{
    // 128-bit UUID beacon
    static const uint8_t beacon_temp_pres[31] = 
    {
        /* Entry for Flags */
        0x02,
        0x01, 0x04,
        /* Entry for Complete List of 128-bit Service Class UUID of Linking beacon */
        0x11,
        0x07, 0xCD, 0xA6, 0x13, 0x5B, 0x83, 0x50, 0x8D, 0x80,0x44, 0x40, 0xD3, 0x50, 0x01, 0x69, 0xB3, 0xB3,
        /* Entry for Manufacture specific data in Linking spec*/
        0x09,
        0xFF, 
        0xE2, 0x02,             // Company ID DoCoMo (0x02E2)
        0x0A, 0xB1, 0x23, 0x45, // Version 0x0, VenderID 0xAB, ClassID 0x12345
        0x00, 0x00              // Service data
    };

    memcpy(&(p_beacon_pdu[3 + M_BD_ADDR_SIZE]), &(beacon_temp_pres[0]), sizeof(beacon_temp_pres));
    p_beacon_pdu[1] = M_BD_ADDR_SIZE + sizeof(beacon_temp_pres);
}


/* Sets the sensor data of the sensor beacon PDU.
 */
static void m_beacon_pdu_sensor_data_set(uint8_t * p_beacon_pdu, uint16_t *p_temperature, uint16_t *p_humidity, uint16_t *p_pressure)
{
    p_beacon_pdu[SINT16_SERVICE_DATA_OFFS] = M_BEACON_PDU_TYPE << 4;  // ServiceID (4-bit)
    if ( M_BEACON_PDU_TYPE == LINKING_SERVICE_TYPE_TEMPERATURE)
    {
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS    ]  = (LINKING_SERVICE_TYPE_TEMPERATURE << 4) & 0xF0;   // Up 4-bits, Service ID
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS    ] |= (*p_temperature >> 8) & 0xF;                      // Low 4-bits (sign and first 3-bit of exponent)
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS + 1]  = (*p_temperature >> 0) & 0xFF;                     // 4th bit of exponent and fraction
        M_BEACON_PDU_TYPE = LINKING_SERVICE_TYPE_HUMIDITY;  // next Humidity
    }
    else if (M_BEACON_PDU_TYPE == LINKING_SERVICE_TYPE_HUMIDITY)
    {
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS    ]  = (LINKING_SERVICE_TYPE_HUMIDITY << 4) & 0xF0;      // Up 4-bits, Service ID
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS    ] |= (*p_humidity >> 8) & 0xF;                         // Low 4-bits (exponent)
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS + 1]  = (*p_humidity >> 0) & 0xFF;                        // fraction
        M_BEACON_PDU_TYPE = LINKING_SERVICE_TYPE_AIRPRESSURE;  // next Air Pressure
    }
    else if (M_BEACON_PDU_TYPE == LINKING_SERVICE_TYPE_AIRPRESSURE)
    {
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS    ]  = (LINKING_SERVICE_TYPE_AIRPRESSURE << 4) & 0xF0;   // Up 4-bits, Service ID
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS    ] |= (*p_pressure >> 8) & 0xF;                         // Low 4-bits (first 4-bit of exponent)
        p_beacon_pdu[SINT16_SERVICE_DATA_OFFS + 1]  = (*p_pressure >> 0) & 0xFF;                        // 5th bit of exponent and fraction
        M_BEACON_PDU_TYPE = LINKING_SERVICE_TYPE_TEMPERATURE;  // next Temperature
    }
}


/* Waits for the next NVIC event.
 */
static void __forceinline cpu_wfe(void)
{
    DBG_WFE_BEGIN;
    __WFE();
    __SEV();
    __WFE();
    DBG_WFE_END;
}


/* Hook for the access mode feature of the lps25h driver.
 */
static void cpu_sleep_hook(void)
{
    cpu_wfe();
}


/* Powers up the the lps25h device and TWI pull-up resistors.
 */
static void sensor_chip_powerup(void)
{
    NRF_GPIO->OUTSET = (1 << 5) | (1 << 8);
    NRF_GPIO->OUTSET = (1 << 7);

    NRF_GPIO->DIRSET = (1 << 5) | (1 << 8);
    NRF_GPIO->DIRSET = (1 << 7);
}


/* Sets up the the lps25h device to measure temperature and pressure.
 */
static bool sensor_chip_measurement_setup(void)
{
    static const drv_lps25h_cfg_t m_drv_lps25h_cfg =
    {
        .twi_id = HAL_TWI_ID_TWI0,
        .twi_cfg.address   = (0x5C << TWI_ADDRESS_ADDRESS_Pos),
        .twi_cfg.frequency = (TWI_FREQUENCY_FREQUENCY_K400 << TWI_FREQUENCY_FREQUENCY_Pos),
        .p_sleep_hook = cpu_sleep_hook,
    };
    
    if ( drv_lps25h_open(&m_drv_lps25h_cfg) == DRV_LPS25H_STATUS_CODE_SUCCESS )
    {
        drv_lps25h_access_mode_set(DRV_LPS25H_ACCESS_MODE_CPU_INACTIVE);
    
        drv_lps25h_ctrl_reg_modify((DRV_LSP25H_CTRL_REG_PD_Active << DRV_LSP25H_CTRL_REG_PD_Pos) |
                                   (DRV_LSP25H_CTRL_REG_ODR_12HZ5 << DRV_LSP25H_CTRL_REG_ODR_Pos), 0);
        
        return ( true );
    }
    else
    {
        (void)drv_lps25h_close();
    }
    
    return ( false );
}


/* Ends after measuring temperature and pressure.
 */
static void sensor_chip_measurement_done(void)
{
    (void)drv_lps25h_close();
}


/* Sends an advertising PDU on the given channel index.
 */
static void send_one_packet(uint8_t channel_index)
{
    uint8_t i;
    
    m_radio_isr_called = false;
    hal_radio_channel_index_set(channel_index);
    hal_radio_send(m_adv_pdu);
    while ( !m_radio_isr_called )
    {
        cpu_wfe();
    }
    
    for ( i = 0; i < 9; i++ )
    {
        __NOP();
    }
}


/* Powers down the the lps25h device and TWI pull-up resistors.
 */
static void sensor_chip_powerdown(void)
{
    NRF_GPIO->DIRCLR = (1 << 5) | (1 << 7) | (1 << 8);
}


/* Handles sensor managing.
 */
static void sensor_handler(uint32_t start_time_us, uint32_t retry_interval_us, uint8_t retry_count)
{
    uint8_t status;

    if ( sensor_chip_measurement_setup() )
    {
        for ( int i = 0; i < retry_count; i++ )
        {
            m_rtc_isr_called = false;
            hal_timer_timeout_set(start_time_us + (i * retry_interval_us));
            while ( !m_rtc_isr_called )
            {
                cpu_wfe();
            }
            drv_lps25h_status_reg_get(&status);
            
            if ( ((status & (DRV_LSP25H_STATUS_REG_T_DA_Available << DRV_LSP25H_STATUS_REG_T_DA_Pos)) != 0)
            &&   ((status & (DRV_LSP25H_STATUS_REG_P_DA_Available << DRV_LSP25H_STATUS_REG_P_DA_Pos)) != 0) )
            {
                break;
            }
        }

        if ( ((status & (DRV_LSP25H_STATUS_REG_T_DA_Available << DRV_LSP25H_STATUS_REG_T_DA_Pos)) != 0)
        &&   ((status & (DRV_LSP25H_STATUS_REG_P_DA_Available << DRV_LSP25H_STATUS_REG_P_DA_Pos)) != 0) )
        {
                if ( M_BEACON_PDU_TYPE == LINKING_SERVICE_TYPE_TEMPERATURE ) 
                {
                    int32_t temperature_milli_deg;
                    drv_lps25h_temperature_get(&temperature_milli_deg);
                    float f_temperature = temperature_milli_deg*0.001f;
                    uint16_t temperature = IEEE754_Convert_Temperature(f_temperature);
                    m_beacon_pdu_sensor_data_set(&(m_adv_pdu[0]), &temperature, NULL, NULL);
                }
                else if ( M_BEACON_PDU_TYPE == LINKING_SERVICE_TYPE_HUMIDITY ) 
                {
                    static float simulated_data_change = 1.0f;

                    simulated_data_change += 1.0f;
                    if (simulated_data_change > 10.0f)
                    {
                      simulated_data_change = 1.0f;
                    }
                    float f_humidity = 155.5f + simulated_data_change;
                    uint16_t humidity = IEEE754_Convert_Humidity(f_humidity);
                    m_beacon_pdu_sensor_data_set(&(m_adv_pdu[0]), NULL, &humidity, NULL);
                }
                else if ( M_BEACON_PDU_TYPE == LINKING_SERVICE_TYPE_AIRPRESSURE ) 
                {
                    uint32_t pressure_pa;
                    drv_lps25h_pressure_get(&pressure_pa);
                    float f_pressure = pressure_pa*0.01f;   //Pa to hPa
                    uint16_t pressure = IEEE754_Convert_Air_Pressure(f_pressure);
                    m_beacon_pdu_sensor_data_set(&(m_adv_pdu[0]), NULL, NULL, &pressure);
                }
        }
        else
        {
            m_beacon_pdu_sensor_data_reset(&(m_adv_pdu[0]));
        }

        sensor_chip_measurement_done();
    }
    sensor_chip_powerdown();
}


/* Handles beacon managing.
 */
static void beacon_handler(void)
{
    hal_radio_reset();
    hal_timer_start();
    
    m_time_us = INITIAL_TIMEOUT - HFCLK_STARTUP_TIME_US; 

    do
    {
        if ( m_skip_read_counter == 0 )
        {
            m_rtc_isr_called = false;
            hal_timer_timeout_set(m_time_us - START_OF_INTERVAL_TO_SENSOR_READ_TIME_US);
            while ( !m_rtc_isr_called )
            {
                cpu_wfe();
            }
            sensor_chip_powerup();
        
            m_rtc_isr_called = false;
            hal_timer_timeout_set(m_time_us - START_OF_INTERVAL_TO_SENSOR_READ_TIME_US + 10000);
            while ( !m_rtc_isr_called )
            {
                cpu_wfe();
            }
            
            sensor_handler(m_time_us - START_OF_INTERVAL_TO_SENSOR_READ_TIME_US + 40000, 10000, 10);
        }
        m_skip_read_counter = ( (m_skip_read_counter + 1) < SENSOR_SKIP_READ_COUNT ) ? (m_skip_read_counter + 1) : 0;
        
        m_rtc_isr_called = false;
        hal_timer_timeout_set(m_time_us);
        while ( !m_rtc_isr_called )
        {
            cpu_wfe();
        }
        hal_clock_hfclk_enable();
        DBG_HFCLK_ENABLED;
        
        m_rtc_isr_called = false;
        m_time_us += HFCLK_STARTUP_TIME_US; 
        hal_timer_timeout_set(m_time_us);
        while ( !m_rtc_isr_called )
        {
            cpu_wfe();
        }
        send_one_packet(37);
        DBG_PKT_SENT;
        send_one_packet(38);
        DBG_PKT_SENT;
        send_one_packet(39);
        DBG_PKT_SENT;
        
        hal_clock_hfclk_disable();
        
        DBG_HFCLK_DISABLED;
        
        m_time_us = m_time_us + (INTERVAL_US - HFCLK_STARTUP_TIME_US); 
    } while ( 1 );
}  


int main(void)
{        
    DBG_BEACON_START;

    NRF_GPIO->OUTCLR = 0xFFFFFFFF;
    NRF_GPIO->DIRCLR = 0xFFFFFFFF;
        
        
#ifdef DBG_WFE_BEGIN_PIN
    NRF_GPIO->OUTSET = (1 << DBG_WFE_BEGIN_PIN);
    NRF_GPIO->DIRSET = (1 << DBG_WFE_BEGIN_PIN);
#endif
    
    hal_serial_init(&serial_cfg);
    hal_twi_init();
    
    drv_lps25h_init();
    
    m_beacon_pdu_init(&(m_adv_pdu[0]));
    m_beacon_pdu_bd_addr_default_set(&(m_adv_pdu[0]));
    m_beacon_pdu_sensor_data_reset(&(m_adv_pdu[0]));
    
#ifdef DBG_RADIO_ACTIVE_ENABLE
    NRF_GPIOTE->CONFIG[0] = (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) 
                          | (DBG_RADIO_ACTIVE_PIN << GPIOTE_CONFIG_PSEL_Pos) 
                          | (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos); 

    NRF_PPI->CH[5].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_OUT[0]); 
    NRF_PPI->CH[5].EEP = (uint32_t)&(NRF_RADIO->EVENTS_READY); 
    NRF_PPI->CH[6].TEP = (uint32_t)&(NRF_GPIOTE->TASKS_OUT[0]); 
    NRF_PPI->CH[6].EEP = (uint32_t)&(NRF_RADIO->EVENTS_DISABLED); 

    NRF_PPI->CHENSET = (PPI_CHEN_CH5_Enabled << PPI_CHEN_CH5_Pos)
                     | (PPI_CHEN_CH6_Enabled << PPI_CHEN_CH6_Pos); 
#endif

    for (;;)
    {
        beacon_handler();
    }
}


void RADIO_IRQHandler(void)
{
    NRF_RADIO->EVENTS_DISABLED = 0;
    m_radio_isr_called = true;    
}


void RTC0_IRQHandler(void)
{
    NRF_RTC0->EVTENCLR = (RTC_EVTENCLR_COMPARE0_Enabled << RTC_EVTENCLR_COMPARE0_Pos);
    NRF_RTC0->INTENCLR = (RTC_INTENCLR_COMPARE0_Enabled << RTC_INTENCLR_COMPARE0_Pos);
    NRF_RTC0->EVENTS_COMPARE[0] = 0;
    
    m_rtc_isr_called = true;    
}
