
#include "cmsis_os2.h"
#include "GUI.h"
#include "DIALOG.h"
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"  // Voor HAL-definities



/* Externe variabelen en functies */
extern osTimerId_t tim_id2;  
extern int timer_cnt;
extern I2C_HandleTypeDef hi2c3;
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




/* --------------------------------------------------------------------- */
/* Sensorfuncties: TC74 uitlezen en temperatuur verzenden via USART      */
/* --------------------------------------------------------------------- */
#define TC74_ADDRESS (0x48<< 1)  // TC74 I2C-adres (7-bits adres, dus shift voor HAL)

#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

// Aangenomen dat deze handles extern gedeclareerd zijn in andere files
extern I2C_HandleTypeDef hi2c3;
extern UART_HandleTypeDef huart1;


// Deze functie checkt of de TC74 reageert op 0x48
void Check_DeviceReady(void) {
  HAL_StatusTypeDef status;
  status = HAL_I2C_IsDeviceReady(&hi2c3, TC74_ADDRESS, 1, 100);
  if (status == HAL_OK) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"TC74 is READY on 0x48\r\n",
                      strlen("TC74 is READY on 0x48\r\n"), HAL_MAX_DELAY);
  } else if (status == HAL_TIMEOUT) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"TC74 TIMEOUT, no response\r\n",
                      strlen("TC74 TIMEOUT, no response\r\n"), HAL_MAX_DELAY);
  } else if (status == HAL_ERROR) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"TC74 ERROR, no ack\r\n",
                      strlen("TC74 ERROR, no ack\r\n"), HAL_MAX_DELAY);
  } else {
    // HAL_BUSY of andere status
    HAL_UART_Transmit(&huart1, (uint8_t*)"TC74 BUSY or unknown\r\n",
                      strlen("TC74 BUSY or unknown\r\n"), HAL_MAX_DELAY);
  }
}

// Deze functie zet standby uit (SHDN=0) in het configregister (0x01)
void TC74_Init(void) {
  uint8_t configData = 0x00;  // bit7 (SHDN)=0 => normal mode
  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
      &hi2c3, TC74_ADDRESS, 0x01, I2C_MEMADD_SIZE_8BIT, &configData, 1, 100);

  if (status == HAL_OK) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"TC74 Init OK\r\n",
                      strlen("TC74 Init OK\r\n"), HAL_MAX_DELAY);
  } else {
    HAL_UART_Transmit(&huart1, (uint8_t*)"TC74 Init FAIL\r\n",
                      strlen("TC74 Init FAIL\r\n"), HAL_MAX_DELAY);
  }
}

// Deze functie leest de temperatuur uit register 0x00
uint8_t TC74_ReadTemperature(void) {
  uint8_t temp = 0xFF;
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c3, TC74_ADDRESS, 0x00, I2C_MEMADD_SIZE_8BIT, &temp, 1, 100);

  if (status == HAL_OK) {
    // (optioneel) debug
    // HAL_UART_Transmit(&huart1, (uint8_t*)"I2C read OK\r\n", 13, HAL_MAX_DELAY);
  } else {
    HAL_UART_Transmit(&huart1, (uint8_t*)"I2C read FAIL\r\n",
                      strlen("I2C read FAIL\r\n"), HAL_MAX_DELAY);
    temp = 0xFF;  // foutcode
  }
  return temp;
}


void USART_SendTemperature(uint8_t value) {
  char msg[50];
  int len = sprintf(msg, "Temperature: %d C\r\n", value);
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 1000);
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
	
  char buf[50];

  GUI_Init();           /* Initialize the Graphics Component */

  /* Add GUI setup code here */
	//GUI_DispString("Hello World!");
	hWin=CreateLogViewer();
	hItem = WM_GetDialogItem(hWin, ID_TEXT_0);
  TEXT_SetText(hItem, "Hello Vives: time = 0");
	

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
			//HAL_UART_Transmit(&huart1, (uint8_t*)"pasage 2", strlen("pasage 2"), HAL_MAX_DELAY);
			 TC74_Init();
			HAL_UART_Transmit(&huart1, (uint8_t*)"Voor Check_DeviceReady\r\n", 25, HAL_MAX_DELAY);
			
			Check_DeviceReady();
			HAL_UART_Transmit(&huart1, (uint8_t*)"Na Check_DeviceReady\r\n", 24, HAL_MAX_DELAY);

      if (status != osOK) {
        //return -1;
			}    			
			start=0;
			//HAL_UART_Transmit(&huart1, (uint8_t*)"pasage 1", strlen("pasage 1"), HAL_MAX_DELAY);

			/* >>> Toegevoegd: Lees direct de temperatuur en verstuur deze via USART <<< */
      

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
		   /* Update de GUI met de nieuwe timerwaarde */
			sprintf(buf, "%d", timer_cnt);
			TEXT_SetText(hItem,buf);
			
			      sprintf(buf, "%d", timer_cnt);
      TEXT_SetText(hItem, buf);

      // Lees de temperatuur van de TC74
      temperature = TC74_ReadTemperature();

      // Print de temperatuur via UART
      sprintf(buf, "Time: %d sec, Temp: %d C\r\n", timer_cnt, temperature);
      HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
			timer_cnt_prev=timer_cnt;
		}


		GUI_TOUCH_Exec();
    GUI_Exec();         /* Execute all GUI jobs ... Return 0 if nothing was done. */
    GUI_X_ExecIdle();   /* Nothing left to do for the moment ... Idle processing */
  }
}
