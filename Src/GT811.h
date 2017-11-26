#ifndef _GT811_H_
#define _GT811_H_

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_i2c.h"
#include "usb_device.h"
#include <stdbool.h>

#include "GT811CONF.h"

// Клон rev3.0, реальное подключение на плате:
// STM32F103C8T6				GT811						Примечание
// 40 - PB4 NJTRST			- 22 - nRSTB
// 41 - PB5 I2C1_SMBA 	- 19 - I2C_SCL 	(подтяжка 10к к VCC)
// 42 - PB6 I2C1_SCL 		- 18 - I2C_SDA 	(подтяжка 10к к VCC)
// 43 - PB7  I2C1_SDA		- 15 - INT 

// требуется переделать к такому виду:
// STM32F103C8T6				GT811						Примечание
// 40 - PB4 NJTRST			- 22 - nRSTB 		(подтяжка 4,7-10к к VCC)
// 41 - PB5 I2C1_SMBA 	- 15 - INT 
// 42 - PB6 I2C1_SCL 		- 19 - I2C_SCL 	(подтяжка 10к к VCC)
// 43 - PB7 I2C1_SDA		- 18 - I2C_SDA 	(подтяжка 10к к VCC)

#define GT811_I2C_TIMEOUT		10

#define GT811_I2C_PIN_SCL   GPIO_PIN_6
#define GT811_I2C_PIN_SDA   GPIO_PIN_7
#define GT811_I2C_PORT   		GPIOB

#define GT811_INT_PIN   		GPIO_PIN_5
#define GT811_INT_PORT   		GPIOB

#define GT811_RST_PIN   		GPIO_PIN_4
#define GT811_RST_PORT   		GPIOB

#define GT811_TOUCH_POINTS 	5

#define GT811_ADDR 					0xBA //0x5d 
#define GT811_REG_CONF 			0x6A2
#define GT811_REG_READ 			0x721

#define GT811_REG_FW_H 			0x717
#define GT811_REG_FW_L 			0x718

void GT811_Init(void);
void GT811_Poll(void);
uint16_t GT811_GetFwVersion(void);

#endif // _GT811_H_
