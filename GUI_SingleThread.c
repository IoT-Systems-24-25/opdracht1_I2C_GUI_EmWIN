
#include "cmsis_os2.h"
#include "GUI.h"
#include "DIALOG.h"
#include <string.h>
#include <stdio.h>

extern osTimerId_t tim_id2;  
extern int timer_cnt;

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
      if (status != osOK) {
        //return -1;
			}    			
			start=0;
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
		  char buf[10];
			sprintf(buf, "%d", timer_cnt);
			TEXT_SetText(hItem,buf);
			timer_cnt_prev=timer_cnt;
		}
		

		GUI_TOUCH_Exec();
    GUI_Exec();         /* Execute all GUI jobs ... Return 0 if nothing was done. */
    GUI_X_ExecIdle();   /* Nothing left to do for the moment ... Idle processing */
  }
}
