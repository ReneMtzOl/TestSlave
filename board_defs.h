#ifndef BOARD_DEFS_H
#define BOARD_DEFS_H

#include <stdint.h>


typedef enum
{
    SLAVE_PWM1 = 1,
    SLAVE_PWM2,
} slave_pwm_id_t;

#define PWM1_GPIO 21
#define PWM2_GPIO 43

#define PWM1_DUTY_PERCENT 30
#define PWM2_DUTY_PERCENT 85

#define MASTER_PWM_ADC0_ID 0
#define MASTER_PWM_ADC1_ID 1

// Placeholder mientras defines rangos reales.
#define PWM1_ADC_MIN_PLACEHOLDER 1
#define PWM1_ADC_MAX_PLACEHOLDER 4095
#define PWM2_ADC_MIN_PLACEHOLDER 1
#define PWM2_ADC_MAX_PLACEHOLDER 4095

// Expected slave ADC ranges for the generic ADC test.
// A grounded ADC input should fail because its value is below the minimum.
#define SLAVE_ADC0_MIN_EXPECTED 1
#define SLAVE_ADC0_MAX_EXPECTED 4095
#define SLAVE_ADC1_MIN_EXPECTED 1
#define SLAVE_ADC1_MAX_EXPECTED 4095
#define SLAVE_ADC2_MIN_EXPECTED 1
#define SLAVE_ADC2_MAX_EXPECTED 4095

// ------------------------------------------------------------
// Slave logical pin IDs.
// These IDs are sent through UART.
// The slave firmware should map these IDs to its real GPIOs.
// ------------------------------------------------------------

typedef enum
{
    SLAVE_SW1_U = 1,
    SLAVE_SW1_D,
    SLAVE_SW2_U,
    SLAVE_SW2_D,
    SLAVE_SW3_U,
    SLAVE_SW3_D,
    SLAVE_SW4_U,
    SLAVE_SW4_D,
    SLAVE_DOOR1_D,
    SLAVE_DOOR2_D,
} slave_switch_id_t;

typedef enum
{
    SLAVE_REL1 = 1,
    SLAVE_REL2,
    SLAVE_REL3,
    SLAVE_REL4,
} slave_relay_id_t;

// ------------------------------------------------------------
// Slave-side GPIO definitions.
// Use these in the slave project.
// ------------------------------------------------------------

#define SW1_U_GPIO 31
#define SW1_D_GPIO 30
#define SW2_U_GPIO 28
#define SW2_D_GPIO 29
#define SW3_U_GPIO 12
#define SW3_D_GPIO 13
#define SW4_D_GPIO 14
#define SW4_U_GPIO 15

#define SW_DOOR1_GPIO 16
#define SW_DOOR2_GPIO 17

#define REL1_GPIO 39
#define REL2_GPIO 22
#define REL3_GPIO 20
#define REL4_GPIO 23

#define BOARD_LED 18

#define I2C_SDA_GPIO 26
#define I2C_SCL_GPIO 27

#define ADC0_GPIO 40
#define ADC1_GPIO 41
#define ADC2_GPIO 42

#endif
