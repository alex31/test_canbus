#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ch.h"
#include "hal.h"
#include "microrl/microrlShell.h"
#include "ttyConsole.h"
#include "stdutil.h"
#include "globalVar.h"
#include "rtcAccess.h"
#include "printf.h"
#include "portage.h"


/*===========================================================================*/
/* START OF EDITABLE SECTION                                           */
/*===========================================================================*/

// declaration des prototypes de fonction
// ces declarations sont necessaires pour remplir le tableau commands[] ci-dessous
static void cmd_mem(BaseSequentialStream *lchp, int argc,const char* const argv[]);
static void cmd_threads(BaseSequentialStream *lchp, int argc,const char* const argv[]);
static void cmd_rtc(BaseSequentialStream *lchp, int argc,const char* const argv[]);
static void cmd_uid(BaseSequentialStream *lchp, int argc,const char* const argv[]);
static void cmd_param(BaseSequentialStream *lchp, int argc,const char* const argv[]);
static void cmd_send(BaseSequentialStream *lchp, int argc,const char* const argv[]);
static void cmd_abrt(BaseSequentialStream *lchp, int argc,const char* const argv[]);

static const ShellCommand commands[] = {
  {"mem", cmd_mem},		// affiche la mémoire libre/occupée
  {"threads", cmd_threads},	// affiche pour chaque thread le taux d'utilisation de la pile et du CPU
  {"rtc", cmd_rtc},		// affiche l'heure contenue par la RTC
  {"uid", cmd_uid},		// affiche le numéri d'identification unique du MCU
  {"param", cmd_param},		// fonction à but pedagogique qui affiche les
				//   paramètres qui lui sont passés
  {"send", cmd_send},		// envoi d'un message CAN
  {"abrt", cmd_abrt},		// annulation d'un message CAN
  {NULL, NULL}			// marqueur de fin de tableau
};


/*
  definition de la fonction cmd_param asociée à la commande param (cf. commands[])
  cette fonction a but pédagogique affiche juste les paramètres fournis, et tente
  de convertir les paramètres en entier et en flottant, et affiche le resultat de
  cette conversion. 
  une fois le programme chargé dans la carte, essayer de rentrer 
  param toto 10 10.5 0x10
  dans le terminal d'eclipse pour voir le résultat 
 */
static void cmd_param(BaseSequentialStream *lchp, int argc,const char* const argv[])
{
  if (argc == 0) {  // si aucun paramètre n'a été passé à la commande param 
    chprintf (lchp, "pas de paramètre en entrée\r\n");
  } else { // sinon (un ou plusieurs pararamètres passés à la commande param 
    for (int argn=0; argn<argc; argn++) { // pour tous les paramètres
      chprintf (lchp, "le parametre %d/%d est %s\r\n", argn+1, argc, argv[argn]); // afficher

      // tentative pour voir si la chaine peut être convertie en nombre entier et en nombre flottant
      int entier = atoi (argv[argn]); // atoi converti si c'est possible une chaine en entier
      float flottant = atof (argv[argn]); // atof converti si c'est possible une chaine en flottant

      chprintf (lchp, "atoi(%s) = %d ;; atof(%s) = %.3f\r\n",
		argv[argn], entier, argv[argn], flottant);
    }
  }
}


static void display_can_slot_avalaibility(void)
{
  CANDriver *canp = &CAND1;
  
  for (uint32_t i=1; i<=CAN_TX_MAILBOXES; i++) {
    chprintf(chp, "[%ld]=%d ", i, can_lld_is_tx_empty(canp, i));
  }
  chprintf(chp, "\r\n");
  const uint32_t code = 1U + ((canp->can->TSR & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos);

  uint32_t mbi;
  for(mbi =CAN_TX_MAILBOXES; mbi > 0; mbi--)
    if(canp->can->TSR  & (CAN_TSR_LOW0 << (mbi-1)))
      break;

  uint32_t index;
  for (index=1; index<=CAN_TX_MAILBOXES; index++)
    if (can_lld_is_tx_empty(canp, index))
      break;
  if (index > CAN_TX_MAILBOXES) {
    DebugTrace("all mailbox busy");
    index = 0;
    if (canp->can->sTxMailBox[index].TIR < canp->can->sTxMailBox[1].TIR)
      index = 1;
    if (canp->can->sTxMailBox[index].TIR < canp->can->sTxMailBox[2].TIR)
      index = 2;
    index++;
  } else {
        DebugTrace("still free mailbox");
  }
  
  DebugTrace("code=%ld; mbi=%ld index=%ld", code, mbi, index);
}

static void cmd_send(BaseSequentialStream *lchp, int argc,const char* const argv[])
{
  if (argc != 2) {  
    chprintf (lchp, "usage : send eid value\r\n");
  } else {
    const CANTxFrame txmsg = {
			      .IDE = CAN_IDE_EXT,
			      .EID = atoi(argv[0]),
			      .RTR = CAN_RTR_DATA,
			      .DLC = 4,
			      .data32[0] = atoi(argv[1]),
			      .data32[1] = 0};
    msg_t status = canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg, chTimeMS2I(200));
    DebugTrace ("canTransmit %s",  (status == MSG_OK) ? "SUCCESS" : "FAIL");
    chThdSleepMilliseconds(100);
    display_can_slot_avalaibility();
  }
}

static void cmd_abrt(BaseSequentialStream *lchp, int argc,const char* const argv[])
{
  if (argc != 1) {  
    chprintf (lchp, "usage : abrt mailbox (or 0 for anymailbox)\r\n");
  } else {
    const uint32_t mailbox = atoi(argv[0]);
    if (mailbox <= CAN_TX_MAILBOXES) {
      canSTM32AbortTransmission(&CAND1, atoi(argv[0]));
      DebugTrace ("can abort query OK");
    } else {
      chprintf (lchp, "usage : abrt mailbox [0 1 2 3]");
    }
    chThdSleepMilliseconds(100);
    display_can_slot_avalaibility();
  }
}



/*
  
 */


/*===========================================================================*/
/* START OF PRIVATE SECTION  : DO NOT CHANGE ANYTHING BELOW THIS LINE        */
/*===========================================================================*/

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/


#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(4096)

#if CONSOLE_DEV_USB == 0
static const SerialConfig ftdiConfig =  {
  115200,
  0,
  USART_CR2_STOP1_BITS | USART_CR2_LINEN,
  0
};
#endif


#define MAX_CPU_INFO_ENTRIES 20
typedef struct _ThreadCpuInfo {
  float    ticks[MAX_CPU_INFO_ENTRIES];
  float    cpu[MAX_CPU_INFO_ENTRIES];
  float    totalTicks;
} ThreadCpuInfo ;

static void stampThreadCpuInfo (ThreadCpuInfo *ti);
static float stampThreadGetCpuPercent (const ThreadCpuInfo *ti, const uint32_t idx);

static void cmd_uid(BaseSequentialStream *lchp, int argc,const char* const argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf (lchp, "Usage: uid\r\n");
    return;
  }

  chprintf (lchp, "uniq id : ");
  for (uint32_t i=0; i< UniqProcessorIdLen; i++)
    chprintf (lchp, "[%x] ", UniqProcessorId[i]);
  chprintf (lchp, "\r\n");
}

static void cmd_rtc(BaseSequentialStream *lchp, int argc,const char* const argv[])
{
  if ((argc != 0) && (argc != 2) && (argc != 6)) {
     DebugTrace ("Usage: rtc [Hour Minute Second Year monTh Day day_of_Week Adjust] value or");
     DebugTrace ("Usage: rtc  Hour Minute Second Year monTh Day");
     return;
  }
 
  if (argc == 2) {
    const char timeVar = (char) tolower ((int) *(argv[0]));
    const int32_t varVal = strtol (argv[1], NULL, 10);
    
    switch (timeVar) {
    case 'h':
      setHour ((uint32_t)(varVal));
      break;
      
    case 'm':
       setMinute ((uint32_t)(varVal));
      break;
      
    case 's':
      setSecond ((uint32_t)(varVal));
      break;
      
    case 'y':
       setYear ((uint32_t)(varVal));
      break;
      
    case 't':
       setMonth ((uint32_t)(varVal));
      break;
      
    case 'd':
       setMonthDay ((uint32_t)(varVal));
      break;

    case 'w':
       setWeekDay ((uint32_t)(varVal));
      break;

    case 'a':
      {
	int32_t newSec =(int)(getSecond()) + varVal;
	if (newSec > 59) {
	  int32_t newMin =(int)(getMinute()) + (newSec/60);
	  if (newMin > 59) {
	    setHour ((getHour()+((uint32_t)(newMin/60))) % 24);
	    newMin %= 60;
	  }
	  setMinute ((uint32_t)newMin);
	}
	if (newSec < 0) {
	  int32_t newMin =(int)getMinute() + (newSec/60)-1;
	  if (newMin < 0) {
	    setHour ((getHour()-((uint32_t)newMin/60)-1) % 24);
	    newMin %= 60;
	  }
	  setMinute ((uint32_t)newMin);
	}
	setSecond ((uint32_t)newSec % 60);
      }
      break;
      
    default:
      DebugTrace ("Usage: rtc [Hour Minute Second Year monTh Day Weekday Adjust] value");
    }
  } else if (argc == 6) {
    setYear ((uint32_t) atoi(argv[3]));
    setMonth ((uint32_t) atoi(argv[4]));
    setMonthDay ((uint32_t) atoi(argv[5]));
    setHour ((uint32_t) atoi(argv[0]));
    setMinute ((uint32_t) atoi(argv[1]));
    setSecond ((uint32_t) atoi(argv[2]));
  }

  chprintf (lchp, "RTC : %s %.02lu/%.02lu/%.04lu  %.02lu:%.02lu:%.02lu\r\n",
	    getWeekDayAscii(), getMonthDay(), getMonth(), getYear(),
	    getHour(), getMinute(), getSecond());
}


static void cmd_mem(BaseSequentialStream *lchp, int argc,const char* const argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf (lchp, "Usage: mem\r\n");
    return;
  }

  chprintf (lchp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf (lchp, "heap free memory : %u bytes\r\n", getHeapFree());

  void * ptr1 = malloc_m (100);
  void * ptr2 = malloc_m (100);

  chprintf (lchp, "(2x) malloc_m(1000) = %p ;; %p\r\n", ptr1, ptr2);
  chprintf (lchp, "heap free memory : %d bytes\r\n", getHeapFree());

  free_m (ptr1);
  free_m (ptr2);
}




static void cmd_threads(BaseSequentialStream *lchp, int argc,const char* const argv[]) {
  static const char *states[] = {THD_STATE_NAMES};
  Thread *tp = chRegFirstThread();
  (void)argv;
  (void)argc;
  float totalTicks=0;
  float idleTicks=0;

  static ThreadCpuInfo threadCpuInfo = {
    .ticks = {[0 ... MAX_CPU_INFO_ENTRIES-1] = 0.f}, 
    .cpu =   {[0 ... MAX_CPU_INFO_ENTRIES-1] =-1.f}, 
    .totalTicks = 0.f
  };
  
  stampThreadCpuInfo (&threadCpuInfo);
  
  chprintf (lchp, "    addr    stack  frestk prio refs  state        time \t percent        name\r\n");
  uint32_t idx=0;
  do {
    chprintf (lchp, "%.8lx %.8lx %6lu %4lu %4lu %9s %9lu   %.1f    \t%s\r\n",
	      (uint32_t)tp, (uint32_t)tp->ctx.sp,
	      get_stack_free (tp),
	      (uint32_t)tp->prio, (uint32_t)(tp->refs - 1),
	      states[tp->state], (uint32_t)tp->time, 
	      stampThreadGetCpuPercent (&threadCpuInfo, idx),
	      chRegGetThreadName(tp));
    totalTicks+= (float) tp->time;
    if (strcmp (chRegGetThreadName(tp), "idle") == 0)
      idleTicks =  (float) tp->time;
    tp = chRegNextThread ((Thread *)tp);
    idx++;
  } while (tp != NULL);

  const float idlePercent = (idleTicks*100.f)/totalTicks;
  const float cpuPercent = 100.f - idlePercent;
  chprintf (lchp, "\r\ncpu load = %.2f%%\r\n", cpuPercent);
}


static const ShellConfig shell_cfg1 = {
#if CONSOLE_DEV_USB == 0
  (BaseSequentialStream *) &CONSOLE_DEV_SD,
#else
  (BaseSequentialStream *) &SDU1,
#endif
  commands
};



void consoleInit (void)
{
  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * USBD1 : FS, USBD2 : HS
   */

#if CONSOLE_DEV_USB != 0
  usbSerialInit(&SDU1, &USBDRIVER); 
  chp = (BaseSequentialStream *) &SDU1;
#else
  sdStart(&CONSOLE_DEV_SD, &ftdiConfig);
  chp = (BaseSequentialStream *) &CONSOLE_DEV_SD;
#endif
  /*
   * Shell manager initialization.
   */
  shellInit();
}


void consoleLaunch (void)
{
  Thread *shelltp = NULL;

 
#if CONSOLE_DEV_USB != 0
   while (TRUE) {
    if (!shelltp) {
      systime_t time=90;


      while (usbGetDriver()->state != USB_ACTIVE) {
	if (time != 100) {
	  time++;
	  chThdSleepMilliseconds(100);
	} else {
	  time=90;
	  //usbSerialReset(&SDU1);
	}
      }
      
      // activate driver, giovani workaround
      chnGetTimeout(&SDU1, TIME_IMMEDIATE);
      while (!isUsbConnected()) {
	chThdSleepMilliseconds(100);
      }
      
      shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
    } else if (shelltp && (chThdTerminated(shelltp))) {
      chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
      shelltp = NULL;           /* Triggers spawning of a new shell.        */
    }
    chThdSleepMilliseconds(100);
  }

#else

   while (TRUE) {
     if (!shelltp) {
       //       palSetPad (BOARD_LED3_P, BOARD_LED3);
       shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
     } else if (chThdTerminated(shelltp)) {
       chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
       shelltp = NULL;           /* Triggers spawning of a new shell.        */
       //       palClearPad (BOARD_LED3_P, BOARD_LED3);
     }
     chThdSleepMilliseconds(100);
   }
   
   
#endif //CONSOLE_DEV_USB

}

static void stampThreadCpuInfo (ThreadCpuInfo *ti)
{
  const Thread *tp =  chRegFirstThread();
  uint32_t idx=0;
  
  float totalTicks =0;
  do {
    totalTicks+= (float) tp->time;
    ti->cpu[idx] = (float) tp->time - ti->ticks[idx];;
    ti->ticks[idx] = (float) tp->time;
    tp = chRegNextThread ((Thread *)tp);
    idx++;
  } while ((tp != NULL) && (idx < MAX_CPU_INFO_ENTRIES));
  
  const float diffTotal = totalTicks- ti->totalTicks;
  ti->totalTicks = totalTicks;
  
  tp =  chRegFirstThread();
  idx=0;
  do {
    ti->cpu[idx] =  (ti->cpu[idx]*100.f)/diffTotal;
    tp = chRegNextThread ((Thread *)tp);
    idx++;
  } while ((tp != NULL) && (idx < MAX_CPU_INFO_ENTRIES));
}


static float stampThreadGetCpuPercent (const ThreadCpuInfo *ti, const uint32_t idx)
{
  if (idx >= MAX_CPU_INFO_ENTRIES) 
    return -1.f;

  return ti->cpu[idx];
}
