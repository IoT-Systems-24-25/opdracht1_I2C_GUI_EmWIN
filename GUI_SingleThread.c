#include "TC74A0.h"
#include "internal_temp.h"
#include "stm32f4xx_hal.h"  // Voor HAL-definities
#include "GUI.h"
#include "DIALOG.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>




/* Externe variabelen en functies */
extern osTimerId_t tim_id2;  
extern int timer_cnt;
extern UART_HandleTypeDef huart1;



#define ID_FRAMEWIN_0     (GUI_ID_USER + 0x00)
#define ID_MULTIEDIT_0     (GUI_ID_USER + 0x01)
#define ID_BUTTON_0     (GUI_ID_USER + 0x02)
#define ID_TEXT_0     (GUI_ID_USER + 0x03)
#define ID_BUTTON_1     (GUI_ID_USER + 0x04)
#define ID_BUTTON_2     (GUI_ID_USER + 0x05)
#define ID_BUTTON_3     (GUI_ID_USER + 0x06)

extern WM_HWIN CreateLogViewer(void);

extern int start;
extern int stop;
extern int pauze;




// Debugprint via UART
static void PrintUART(const char *text) {
  HAL_UART_Transmit(&huart1, (uint8_t*)text, strlen(text), HAL_MAX_DELAY);
}



/*----------------------------------------------------------------------------
 *      GUIThread: GUI Thread for Single-Task Execution Model
 *---------------------------------------------------------------------------*/
#define GUI_THREAD_STK_SZ    (2048U)

static void         GUIThread (void *argument);         /* thread function */
static osThreadId_t GUIThread_tid;                      /* thread id */
static uint64_t     GUIThread_stk[GUI_THREAD_STK_SZ/8]; /* thread stack */

static const osThreadAttr_t GUIThread_attr = {
  .stack_mem  = &GUIThread_stk[0],
  .stack_size = sizeof(GUIThread_stk),
  .priority   = osPriorityIdle 
};

int Init_GUIThread (void) {

  GUIThread_tid = osThreadNew(GUIThread, NULL, &GUIThread_attr);
  if (GUIThread_tid == NULL) {
    return(-1);
  }

  return(0);
}

__NO_RETURN static void GUIThread (void *argument) {
  (void)argument;
	WM_HWIN hWin,hItem;
	int pauze_toestand;
	osStatus_t status; 
	int timer_cnt_prev=0;
	uint8_t temperature;
  float temperatureMCU;
  char buf[64];

  GUI_Init();           /* Initialize the Graphics Component */

  /* Add GUI setup code here */
	//GUI_DispString("Hello World!");
	hWin=CreateLogViewer();
	hItem = WM_GetDialogItem(hWin, ID_TEXT_0);
  TEXT_SetText(hItem, "Hello Vives: time = 0");
	
  // Init de CMSIS I2C-driver
  if (TC74_DriverInitialize() == 0) {
    PrintUART("I2C driver init OK\r\n");
  } else {
    PrintUART("I2C driver init FAIL\r\n");
  }
	
	// 3) Init ADC voor interne sensor
  InternalTemp_ADC_Init();
  PrintUART("Internal ADC init done\r\n");
	
  while (1) {
  
    /* All GUI related activities might only be called from here */
		
		if(start==1)
		{			
		   
			//reset teller als niet in pauze toestand
			//pauze-toestand verlaten
			if(pauze_toestand==0)
				timer_cnt=0;
			pauze_toestand=0;
			//start timer
			//start timer with periodic 1000ms interval
      status = osTimerStart(tim_id2, 1000U); 
      if (status != osOK) {
        //return -1;
			}    			
			start=0;
			
			
      // Check device
      if (TC74_CheckReady() == 0) {
        PrintUART("TC74 is READY on 0x48\r\n");
      } else {
        PrintUART("TC74 NO ACK on 0x48\r\n");
      }
      // Init sensor
      if (TC74_Init() == 0) {
        PrintUART("TC74 Init OK\r\n");
      } else {
        PrintUART("TC74 Init FAIL\r\n");
      }

		}
		if(stop==1)
		{
			//stop timer
			status=osTimerStop(tim_id2);
			 if (status != osOK) {
        //return -1;
			}   
		  stop=0;
		}
		if(pauze==1)
		{
			//stop timer
			status=osTimerStop(tim_id2);
			 if (status != osOK) {
        //return -1;
			}   
			pauze_toestand=1;
			pauze=0;
		}
		if(timer_cnt!=timer_cnt_prev)
		{
			 // Lees temp  Externe sensor (TC74)
      if (TC74_ReadTemperature(&temperature) == 0) {
        sprintf(buf, "Time: %d sec, Temp: %d C\r\n", timer_cnt, temperature);
        PrintUART(buf);
				TEXT_SetText(hItem,buf);
      } else {
        PrintUART("TC74 Read FAIL\r\n");
      }
			
			// 5) Interne sensor (MCU)
      temperatureMCU = InternalTemp_ReadCelsius();
      sprintf(buf, "Time: %d sec, MCU Temp: %.2f C\r\n", timer_cnt, temperatureMCU);
      PrintUART(buf);
			TEXT_SetText(hItem,buf);

			timer_cnt_prev=timer_cnt;
		}


		GUI_TOUCH_Exec();
    GUI_Exec();         /* Execute all GUI jobs ... Return 0 if nothing was done. */
    GUI_X_ExecIdle();   /* Nothing left to do for the moment ... Idle processing */
  }
}
