/*****************************************************************************
 * | File      	:   dev_config.h
 * | Author      :   Waveshare team
 * | Function    :   Hardware underlying interface
 * | Info        :
 *----------------
 * |	This version:   V1.0
 * | Date        :   2025-08-29
 * | Info        :   Basic version
 *
 ******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_uart.h"
#include "tkl_pinmux.h"

#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#define EXAMPLE_PWR_BUTTON_NAME "btn_pwr"

// Power Button GPIO
#ifndef EXAMPLE_SYS_PWR_PIN
#define EXAMPLE_SYS_PWR_PIN TUYA_GPIO_NUM_18
#endif

// Power Domain Enable GPIO
#ifndef EXAMPLE_SYS_EN_PIN
#define EXAMPLE_SYS_EN_PIN TUYA_GPIO_NUM_19
#endif

// GPS I2C Pins (I2C Port 0 - GPIO 20/21) - Shared with touch display
#ifndef GPS_I2C_SCL_PIN
#define GPS_I2C_SCL_PIN TUYA_GPIO_NUM_20
#endif
#ifndef GPS_I2C_SDA_PIN
#define GPS_I2C_SDA_PIN TUYA_GPIO_NUM_21
#endif

// BMM150/BNO08x Magnetometer/IMU I2C Pins (I2C Port 2 - GPIO 14/15)
// Note: BMM150 and BNO08x share the same I2C port (only one can be used at a time)
#ifndef BMM150_I2C_SCL_PIN_NUM
#define BMM150_I2C_SCL_PIN_NUM TUYA_GPIO_NUM_14
#endif
#ifndef BMM150_I2C_SDA_PIN_NUM
#define BMM150_I2C_SDA_PIN_NUM TUYA_GPIO_NUM_15
#endif

// BNO08x uses the same I2C pins and port as BMM150
#define BNO08X_I2C_SCL_PIN_NUM BMM150_I2C_SCL_PIN_NUM
#define BNO08X_I2C_SDA_PIN_NUM BMM150_I2C_SDA_PIN_NUM

// GPS UART Pins (UART Port 2 - GPIO 40/41) - Alternative to I2C
#ifndef GPS_UART_TX_PIN
#define GPS_UART_TX_PIN TUYA_GPIO_NUM_41 // P41 for UART2_TX
#endif
#ifndef GPS_UART_RX_PIN
#define GPS_UART_RX_PIN TUYA_GPIO_NUM_40 // P40 for UART2_RX
#endif

// Legacy compatibility - defaults to GPS pins
#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN GPS_I2C_SCL_PIN
#endif
#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN GPS_I2C_SDA_PIN
#endif

OPERATE_RET dev_gpio_init(uint8_t pin, uint8_t mode);
OPERATE_RET dev_sys_init();

OPERATE_RET dev_digital_write(uint8_t pin, uint8_t value);
OPERATE_RET dev_digital_read(uint8_t pin, uint8_t *value);

// GPS I2C functions (uses I2C port 0 with GPIO 20/21) - shared with touch display
OPERATE_RET dev_i2c_init();
OPERATE_RET dev_i2c_write(uint8_t addr, uint8_t reg, uint8_t value);
OPERATE_RET dev_i2c_write_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len);
OPERATE_RET dev_i2c_read_nbytes(uint8_t addr, uint8_t reg, uint8_t *pdata, uint32_t len);
OPERATE_RET dev_i2c_read_only_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len);

// BMM150/BNO08x I2C functions (uses I2C Port 2 with GPIO 14/15)
// Note: These functions can be used by both BMM150 and BNO08x drivers
OPERATE_RET bmm150_i2c_port_init();  // Initializes I2C Port 2 for magnetometer/IMU
OPERATE_RET bmm150_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value);
OPERATE_RET bmm150_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length);

// GPS UART functions (for UART interface mode)
OPERATE_RET dev_uart_init(TUYA_UART_NUM_E port, uint32_t baudrate);
OPERATE_RET dev_uart_deinit(TUYA_UART_NUM_E port);
int dev_uart_write(TUYA_UART_NUM_E port, const uint8_t *data, uint16_t len);
int dev_uart_read(TUYA_UART_NUM_E port, uint8_t *data, uint16_t len, uint32_t timeout_ms);

#endif
