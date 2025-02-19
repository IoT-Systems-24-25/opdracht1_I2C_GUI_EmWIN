#include "TC74A0.h" 				 // Externe sensor (TC74) via CMSIS I2C
#include "internal_temp.h"   // Interne ADC-sensor module
#include "stm32f4xx_hal.h"  // Voor HAL-definities
#include "GUI.h"
#include "DIALOG.h"
#include "PROGBAR.h"        // Voor progressbar-functies
#include "SLIDER.h"          // Voor slider-functies
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include "Board_LED.h"      // Voor LED_On/LED_Off
#include "USBH_MSC.h"        // USB MSC API
#include "rl_fs.h"           // File System API (fopen, fprintf, fclose)



#define ID_FRAMEWIN_0     (GUI_ID_USER + 0x00)
#define ID_BUTTON_0     (GUI_ID_USER + 0x01)  // toon temperatuur button
#define ID_PROGBAR_0     (GUI_ID_USER + 0x02) // progresbar interne temp
#define ID_PROGBAR_1     (GUI_ID_USER + 0x03) // progresbar externe temp
#define ID_BUTTON_1     (GUI_ID_USER + 0x04)  // save naar usb button
#define ID_TEXT_0     (GUI_ID_USER + 0x05)    // interne temp textveld
#define ID_TEXT_1     (GUI_ID_USER + 0x06)    // externe temp textveld 
#define ID_MULTIEDIT_0     (GUI_ID_USER + 0x07) // toon intern temp
#define ID_MULTIEDIT_1     (GUI_ID_USER + 0x08) // toon externe temp 
#define ID_BUTTON_2     (GUI_ID_USER + 0x09)    // auto button
#define ID_BUTTON_3     (GUI_ID_USER + 0x0A)    //manual button
#define ID_EDIT_0     (GUI_ID_USER + 0x0B)      //simulatie ventilator
#define ID_SLIDER_0     (GUI_ID_USER + 0x0C)    // temp instellen start ventilator
#define ID_EDIT_1     (GUI_ID_USER + 0x0D)      // waarde waar limit staat 

/* Externe variabelen en functies */
extern osTimerId_t tim_id2;  
extern int timer_cnt;
extern UART_HandleTypeDef huart1;
extern int ToonTemperatuur;
extern int SaveNaarUsb;
int extern Auto;
int extern Manueel;
extern WM_HWIN CreateLogViewer(void);

static int TempLimit = 25;  // standaard limiet op 25°C





// Debugprint via UART
static void PrintUART(const char *text) {
  HAL_UART_Transmit(&huart1, (uint8_t*)text, strlen(text), HAL_MAX_DELAY);
}

static void LogTemperatureToUSB(uint8_t tempTC74, float tempMCU, int timerValue) {
  const char *drive = "U0:";
  int retry = 3; // Probeer maximaal 3 keer

  while (retry--) {
    if (USBH_MSC_DriveGetMediaStatus(drive) == USBH_MSC_OK) {
      int32_t mount_status = USBH_MSC_DriveMount(drive);
      if (mount_status == USBH_MSC_OK) {
        FILE *fp = fopen("U0:/temp_log.txt", "a");
        if (fp != NULL) {
          fprintf(fp, "Time: %d sec, TC74: %d C, MCU: %.2f C\r\n", timerValue, tempTC74, tempMCU);
          fclose(fp);
          PrintUART("Temperatuur gelogd naar USB\r\n");
          return;  // Succesvol gelogd, exit de functie
        } else {
          PrintUART("File open FAIL\r\n");
        }
      } else if (mount_status == USBH_MSC_ERROR_FORMAT) {
        PrintUART("USB drive mounted maar onformatteerd, format nodig\r\n");
        // Voer formatactie uit (controleer of fformat() beschikbaar is)
        fformat(drive, "/FAT32");
      } else {
        PrintUART("USB drive mount FAIL\r\n");
      }
    } else {
      PrintUART("USB drive not connected\r\n");
    }
    // Wacht 500ms en probeer opnieuw
    osDelay(500);
  }
  // Als hier aangekomen, dan is loggen niet gelukt.
  PrintUART("USB logging FAILED na retries\r\n");
}


static void USBLoggerThread(void *argument) {
  (void)argument;
  uint8_t temperature;
  float temperatureMCU;
  char tempextern[64];
  char tempintern[64];

  // Lees sensorwaarden
  if (TC74_ReadTemperature(&temperature) == 0) {
    sprintf(tempextern, "Temp: %d C", temperature);
  } else {
    strcpy(tempextern, "TC74 Read FAIL");
  }
  temperatureMCU = InternalTemp_ReadCelsius();
  sprintf(tempintern, "Temp: %.2f C", temperatureMCU);

  // Log naar USB
  LogTemperatureToUSB(temperature, temperatureMCU, timer_cnt);

  // Beëindig deze thread
  osThreadExit();
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
	WM_HWIN hWin,ToonInternTemp,ToonExternTemp, hProgInt, hProgExt, Ventilator, hSlider, hEditLimit;
	int pauze_toestand;
	osStatus_t status; 
	int timer_cnt_prev=0;
	uint8_t temperature;
  float temperatureMCU;
  char tempextern[64];
	char tempintern[64];
	char limitText[32];

  GUI_Init();           /* Initialize the Graphics Component */

  /* Add GUI setup code here */
	//GUI_DispString("Hello World!");
	hWin=CreateLogViewer();
	
	ToonInternTemp = WM_GetDialogItem(hWin, ID_MULTIEDIT_0);
	ToonExternTemp = WM_GetDialogItem(hWin, ID_MULTIEDIT_1);
	Ventilator     = WM_GetDialogItem(hWin, ID_EDIT_0 );
	hProgInt       = WM_GetDialogItem(hWin, ID_PROGBAR_0);  // Interne temperatuur progressbar
  hProgExt       = WM_GetDialogItem(hWin, ID_PROGBAR_1);  // Externe temperatuur progressbar
	hSlider        = WM_GetDialogItem(hWin, ID_SLIDER_0);
  hEditLimit     = WM_GetDialogItem(hWin, ID_EDIT_1);
	SLIDER_SetRange(hSlider, 0, 100);                       // stel het bereik in (bijv. 0 tot 100)
  SLIDER_SetValue(hSlider, 25);                           // standaardwaarde op 25 graden
 
  

 
	
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
  
    // Lees sliderwaarde en update het limit edit-veld
    TempLimit = SLIDER_GetValue(hSlider);
    sprintf(limitText, " start ventilator : %d C", TempLimit);
    EDIT_SetText(hEditLimit, limitText);
		
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
			Manueel = 0;
			Auto = 0;
		  osThreadNew(USBLoggerThread, NULL, NULL);
			//stop timer
			status=osTimerStop(tim_id2);
			 if (status != osOK) {
        //return -1;
			}   
			pauze_toestand=1;
			SaveNaarUsb=0;
			
		}
		
				
// Eerst controleren of de manueel-modus actief is:
if (Manueel == 1) {
    // Zorg dat de auto-modus wordt uitgeschakeld:
    Auto = 0;
    
    // Gebruik een statische variabele om de huidige LED-status bij te houden
    static int ledState = 0;  // 0 = LED uit, 1 = LED aan
    // Toggle de LED-status:
    ledState = !ledState;
    if (ledState) {
        LED_On(0);
        PrintUART("LED LD3 manueel aan \r\n");
			  EDIT_SetText(Ventilator,"LD3 aan");
    } else {
        LED_Off(0);
        PrintUART("LED LD3 manueel uit\r\n");
			EDIT_SetText(Ventilator,"LD3 uit");
    }
    // Reset de manueel-flag zodat de toggle maar één keer per druk gebeurt
    Manueel = 0;
}
// Als manueel niet actief is, en de auto-modus wel:
else if (Auto == 1) {
    // Bij auto wordt de LED aangestuurd op basis van de temperatuur:
    if (temperature > TempLimit) {
        LED_On(0);
			EDIT_SetText(Ventilator,"LD3 aan");
    } else {
        LED_Off(0);
			EDIT_SetText(Ventilator,"LD3 uit");
    }
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
