#include "Driver_I2C.h"       // CMSIS driver interface
#include "Driver_Common.h"    // ARM_DRIVER_OK, etc.
#include "cmsis_os2.h"
#include "GUI.h"
#include "DIALOG.h"
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"  // Voor HAL-definities



/* Externe variabelen en functies */
extern osTimerId_t tim_id2;  
extern int timer_cnt;
extern ARM_DRIVER_I2C Driver_I2C3;   // Gedefinieerd in I2C_STM32F4xx.c
static ARM_DRIVER_I2C *ptrI2C = &Driver_I2C3;
extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef hadc1;  


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





// 7-bit adres van de TC74A0 is 0x48
#define TC74_7BIT_ADDR   (0x48)
#define REG_CONFIG       0x01
#define REG_TEMP         0x00


// Debugprint via UART
static void PrintUART(const char *text) {
  HAL_UART_Transmit(&huart1, (uint8_t*)text, strlen(text), HAL_MAX_DELAY);
}

/**************************************************************
 * 1) CMSIS I2C-driver initialisatie
 **************************************************************/
int32_t TC74_DriverInitialize(void)
{
    int32_t status;

    // Initialize de driver
    status = ptrI2C->Initialize(NULL); // NULL = geen callback
    if (status != ARM_DRIVER_OK) {
        return -1;
    }

    // PowerControl
    status = ptrI2C->PowerControl(ARM_POWER_FULL);
    if (status != ARM_DRIVER_OK) {
        return -2;
    }

    // Zet bus speed op Standard (100kHz)
    status = ptrI2C->Control(ARM_I2C_BUS_SPEED_STANDARD, 0);
    if (status != ARM_DRIVER_OK) {
        return -3;
    }

    return 0; // OK
}


/**************************************************************
 * 2) Check_DeviceReady (CMSIS-stijl)
 *    We doen MasterTransmit van 1 dummy-byte en checken DataCount
 **************************************************************/
int32_t TC74_CheckReady(void)
{
    uint8_t dummy[1] = {0x00};
    ptrI2C->MasterTransmit(TC74_7BIT_ADDR, dummy, 1, false);
    while (ptrI2C->GetStatus().busy);

    int count = ptrI2C->GetDataCount();
    if (count == 1) {
        return 0; // device ack
    }
    return -1;    // no ack
}


/**************************************************************
 * 3) TC74_Init: zet register 0x01 op 0x00 (SHDN=0 => normal mode)
 **************************************************************/
int32_t TC74_Init(void)
{
    // We sturen 2 bytes: [reg, data]
    uint8_t tx[2];
    tx[0] = REG_CONFIG; // 0x01
    tx[1] = 0x00;       // Normal mode

    ptrI2C->MasterTransmit(TC74_7BIT_ADDR, tx, 2, false);
    while (ptrI2C->GetStatus().busy);

    if (ptrI2C->GetDataCount() != 2) {
        return -1;
    }
    return 0;
}

/**************************************************************
 * 4) TC74_ReadTemperature: lees 1 byte uit register 0x00
 **************************************************************/
int32_t TC74_ReadTemperature(uint8_t *outTemp)
{
    // 1) Stuur registeradres (0x00)
    uint8_t reg = REG_TEMP;
    ptrI2C->MasterTransmit(TC74_7BIT_ADDR, &reg, 1, true); // repeatedStart
    while (ptrI2C->GetStatus().busy);
    if (ptrI2C->GetDataCount() != 1) {
        return -1;
    }

    // 2) Lees 1 byte
    ptrI2C->MasterReceive(TC74_7BIT_ADDR, outTemp, 1, false);
    while (ptrI2C->GetStatus().busy);
    if (ptrI2C->GetDataCount() != 1) {
        return -2;
    }

    return 0;
}


/*------------------------------------------------------------------------------
 * 3) Interne sensor: STM32F4 via ADC
 *----------------------------------------------------------------------------*/


// 3.1) Init-functie (handmatig of via CubeMX)
void InternalTemp_ADC_Init(void)
{
  __HAL_RCC_ADC1_CLK_ENABLE(); // Enable ADC1 clock

  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance                   = ADC1;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode          = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;

  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    PrintUART("InternalTemp_ADC_Init FAIL\r\n");
  }

  // Channel 16 => interne sensor
  sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank         = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES; // STM32F4: lange sample time
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    PrintUART("ADC ConfigChannel FAIL\r\n");
  }
}

// 3.2) Lees en bereken de interne chip-temperatuur
float InternalTemp_ReadCelsius(void)
{
  // 1) Start
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK) {
    return -999.0f; // Fout of timeout
  }

  // 2) Ruwe ADC
  uint32_t raw = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);

  // 3) Reken om (STM32F4 typical)
  // T = ((Vsense - 0.76) / 0.0025) + 25
  float vsense = (float)raw * 3.3f / 4095.0f;
  float temperature = ((vsense - 0.76f) / 0.0025f) + 25.0f;
  return temperature;
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
		   /* Update de GUI met de nieuwe timerwaarde */
		//	sprintf(buf, "%d", timer_cnt);
		//	TEXT_SetText(hItem,buf);
			
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
