#include "TC74A0.h" 				 // Externe sensor (TC74) via CMSIS I2C
#include "internal_temp.h"   // Interne ADC-sensor module
#include "stm32f4xx_hal.h"  // Voor HAL-definities
#include "GUI.h"
#include "DIALOG.h"
#include "PROGBAR.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>


#define ID_FRAMEWIN_0            (GUI_ID_USER + 0x00)
#define ID_BUTTON_0            (GUI_ID_USER + 0x07)  // toon temperatuur button
#define ID_PROGBAR_0     (GUI_ID_USER + 0x02) // progresbar interne temp
#define ID_PROGBAR_1     (GUI_ID_USER + 0x03) // progresbar externe temp
#define ID_BUTTON_1            (GUI_ID_USER + 0x0A) // save naar usb button
#define ID_TEXT_0            (GUI_ID_USER + 0x0B)  // interne temp textveld
#define ID_TEXT_1            (GUI_ID_USER + 0x0C)  // externe temp textveld 
#define ID_MULTIEDIT_0     (GUI_ID_USER + 0x07)  // toon intern temp
#define ID_MULTIEDIT_1     (GUI_ID_USER + 0x08)  // toon externe temp 


/* Externe variabelen en functies */
extern osTimerId_t tim_id2;  
extern int timer_cnt;
extern UART_HandleTypeDef huart1;
extern int ToonTemperatuur;
extern int SaveNaarUsb;
extern WM_HWIN CreateLogViewer(void);






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
	WM_HWIN hWin,ToonInternTemp,ToonExternTemp, hProgInt, hProgExt;
	int pauze_toestand;
	osStatus_t status; 
	int timer_cnt_prev=0;
	uint8_t temperature;
  float temperatureMCU;
  char tempextern[64];
	char tempintern[64];

  GUI_Init();           /* Initialize the Graphics Component */

  /* Add GUI setup code here */
	//GUI_DispString("Hello World!");
	hWin=CreateLogViewer();
	
	ToonInternTemp = WM_GetDialogItem(hWin, ID_MULTIEDIT_0);
	ToonExternTemp = WM_GetDialogItem(hWin, ID_MULTIEDIT_1);
	hProgInt = WM_GetDialogItem(hWin, ID_PROGBAR_0);  // Interne temperatuur progressbar
  hProgExt = WM_GetDialogItem(hWin, ID_PROGBAR_1);  // Externe temperatuur progressbar
 
	
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
		
		if(ToonTemperatuur==1)
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
			ToonTemperatuur=0;
			
			
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
		
		if(SaveNaarUsb==1)
		{
			//stop timer
			status=osTimerStop(tim_id2);
			 if (status != osOK) {
        //return -1;
			}   
			pauze_toestand=1;
			SaveNaarUsb=0;
		}
		
		if(timer_cnt!=timer_cnt_prev)
		{
			 // Lees temp  Externe sensor (TC74)
      if (TC74_ReadTemperature(&temperature) == 0) {
        sprintf(tempextern,"Temp: %d C\r\n", temperature);
        PrintUART(tempextern);
			MULTIEDIT_SetText(ToonExternTemp,tempextern);
				        // Stel de externe progressbar in (bijvoorbeeld 0 tot 100)
        PROGBAR_SetValue(hProgExt, (int)temperature);
      } else {
        PrintUART("TC74 Read FAIL\r\n");
      }
			
			// 5) Interne sensor (MCU)
      temperatureMCU = InternalTemp_ReadCelsius();
      sprintf(tempintern, "Temp: %.2f C\r\n", temperatureMCU);
      PrintUART(tempintern);
			MULTIEDIT_SetText(ToonInternTemp,tempintern);
			 // Stel de interne progressbar in (bijvoorbeeld 0 tot 100)
			PROGBAR_SetValue(hProgInt, (int)temperatureMCU);

			timer_cnt_prev=timer_cnt;
		}


		GUI_TOUCH_Exec();
    GUI_Exec();         /* Execute all GUI jobs ... Return 0 if nothing was done. */
    GUI_X_ExecIdle();   /* Nothing left to do for the moment ... Idle processing */
  }
}
