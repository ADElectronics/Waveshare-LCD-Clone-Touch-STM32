#include "GT811.h"
#include "usb_device.h"
//#include "usbd_customhid.h"
#include "usbd_custom_hid_if.h"

uint8_t GT811_Data[34];
uint32_t GT811_I2C_Timeout;

extern USBD_HandleTypeDef hUsbDeviceFS;
extern I2C_HandleTypeDef hi2c1;

extern void GT811_RegRead(uint16_t reg, uint8_t size, uint8_t *data);
extern void GT811_RegWrite(uint16_t reg, uint8_t size, uint8_t *data);

struct GT811_TouchReport
{
    uint8_t report[11];
    bool touch_up_sent;
};

struct GT811_TouchReport GT811_TouchReports[GT811_TOUCH_POINTS];
struct GT811_TouchReport *hid_report;

__inline void GT811_RegRead(uint16_t reg, uint8_t size, uint8_t *data)
{
	HAL_I2C_Mem_Read(&hi2c1, GT811_ADDR, reg, I2C_MEMADD_SIZE_16BIT, data, size, GT811_I2C_TIMEOUT);
}

__inline void GT811_RegWrite(uint16_t reg, uint8_t size, uint8_t *data)
{
	HAL_I2C_Mem_Write(&hi2c1, GT811_ADDR, reg, I2C_MEMADD_SIZE_16BIT, data, size, GT811_I2C_TIMEOUT);
}

void GT811_Init(void)
{
	uint8_t currentFinger;
	
	// Reset GT811
	HAL_GPIO_WritePin(GT811_RST_PORT, GT811_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(200);	
	HAL_GPIO_WritePin(GT811_RST_PORT, GT811_RST_PIN, GPIO_PIN_SET);
	HAL_Delay(300);	
	
	GT811_RegWrite(GT811_REG_CONF, sizeof(GT811_Config), (uint8_t*)GT811_Config);

	for(currentFinger = 0; currentFinger < GT811_TOUCH_POINTS; currentFinger++)
	{
		hid_report = &GT811_TouchReports[currentFinger];
		hid_report->touch_up_sent = true;
	}
}

uint16_t GT811_GetFwVersion(void)
{
	uint8_t FW[2];
	
	GT811_RegRead(GT811_REG_FW_H, 2, FW);
	
	return FW[1] + (FW[0] << 8);
}

void GT811_Poll(void)
{
	uint8_t i, currentFinger, validTouchPoints, offsetData, sentReports;
	uint16_t y_value, inverted_y, scan_time;
	
	  if (HAL_GPIO_ReadPin(GT811_INT_PORT, GT811_INT_PIN) == GPIO_PIN_RESET) 
		{
        for(i = 0; i < sizeof(GT811_Data); i++)
				{
            GT811_Data[i] = 0; 
				}
				
        // read the current touch report
        GT811_RegRead(GT811_REG_READ, 34, GT811_Data);

        validTouchPoints = 0;
        sentReports = 0;

        // find out how many touches we have:
        for(currentFinger = 0; currentFinger < GT811_TOUCH_POINTS; currentFinger++)
        {  
            // check if the "finger N" bit is set high in GT811 answer
            if((GT811_Data[0] & (1 << currentFinger)) > 0) 
            {
                validTouchPoints++;
            }
            else if(hid_report->touch_up_sent == false)
            {
                // last frame contained a contact point for this finger;
                // => we must send a "touch up" report, which is considered
                // a valid touch point...
                validTouchPoints++;
            }
        }

        // we need to get rid of the stupid "reserved" (0x733 to 0x739) 
        // bytes in the GT811 touch reports for easier access later
        // so we shift them accordingly
        for(i = 24; i < 34; i++) 
        {
            GT811_Data[i - 6] = GT811_Data[i];
            GT811_Data[i] = 0;
        }

        // send one report per finger found
        for(currentFinger = 0; currentFinger < GT811_TOUCH_POINTS; currentFinger++)
        {          
            // point to the current finger's report  
            hid_report = &GT811_TouchReports[currentFinger];

            // Report ID
            hid_report->report[0] = 0x01;

            // check if the "finger N" bit is set high in GT811 answer
            if((GT811_Data[0] & (1 << currentFinger)) > 0) 
            {
                // byte[1] = state        
                // .......x: Tip switch
                // xxxxxxx.: Ignored
                hid_report->report[1] = 1;

                // this is still within a move sequence of reports
                hid_report->touch_up_sent = false;
            }
            else if(hid_report->touch_up_sent == false)
            {
                // we're required to send a "touch up" report when
                // a finger was lifted. This means that after the last
                // report with TIP = true was sent we need to issue
                // yet another report for the same finger with the 
                // last coordinates but TIP = false
                hid_report->report[1] = 0;
                
                // taking care of sending only one touch up report
                hid_report->touch_up_sent = true;
            }
            else
						{
                continue;   // skip this "finger"
						}
            
            if(hid_report->touch_up_sent == false)
            {            
                // calculate the "data offset" in the GT811 data_align
                // for the current finger / at 5 bytes per finger
                offsetData = currentFinger * 5; 

                // byte[2] Contact index
                hid_report->report[2] = currentFinger;

                // byte[3] Tip pressure
                hid_report->report[3] = GT811_Data[offsetData + 6];

                // NOTE: apparently the waveshare touch digitizer was mounted
                // mirrored in both directions, this means we "flip" X <-> Y
                // coordinates AND we have to mirror the Y axis

                // byte[4] X coordinate LSB                
                hid_report->report[4] = GT811_Data[offsetData + 5];
                
                // byte[5] X coordinate MSB 
                hid_report->report[5] = GT811_Data[offsetData + 4];

                // Y is inverted for some reason
                // so we have to do some math here...
                // basicall y max - value...
                y_value =  GT811_Data[offsetData + 3] + (GT811_Data[offsetData + 2] << 8);
                inverted_y = 480 - y_value;

                // byte[6] Y coordinate LSB
                hid_report->report[6] = inverted_y & 0xFF;

                // byte[7] Y coordinate MSB
                hid_report->report[7] = (uint8_t)(inverted_y >> 8) ;    

                // byte[8] contact count  
                if(sentReports == 0) 
                {
                    // this is the first report in the frame: write touch count in report
                    hid_report->report[10] = validTouchPoints;   
                }                    
                else
								{
                    hid_report->report[10] = 0;
								}
            }

            // report's scan time
						scan_time = (uint16_t)(HAL_GetTick() & 0xFFFF);
            hid_report->report[8] = scan_time & 0xFF;
            hid_report->report[9] = (uint8_t)(scan_time >> 8);

						USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, hid_report->report, 11);
						HAL_Delay(1); // мы не следим, отправился ли Report, поэтому просто подождем 1мс
            sentReports++;
        }            
    }
}
