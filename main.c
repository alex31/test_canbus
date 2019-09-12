/*
  Nom(s), prénom(s) du ou des élèves : 

  QUESTION 1 : influence de la suppression du tableau prendDeLaPlaceSurLaPile ?

 */
#include <ch.h>
#include <hal.h>
#include "stdutil.h"		// necessaire pour initHeap
#include "ttyConsole.h"		// fichier d'entête du shell
#include "stdnoreturn.h"


#define ROLE_RECEIVER 0x2
#define ROLE_TRANSMITTER 0x4

/*
* horloge source PCLK1 : 54Mhz
pour 500Kbaud
 * 1+9+2 = 12 time quantum
 * prescaler = 9
 * f = 54e6 / (9*12) = 500KBaud
========
 pour 1Mbaud
  * 1+6+2 = 9 time quantum
  * prescaler = 6
  * 54e6 / (6*9) = 1MBaud
*/

/*0b100 - Data with EID or (0b110 - RemoteFrame with EID)*/
#define SET_CAN_EID_DATA(x) (((x) << 3)|0b100)

/*0b110 - Mask enable for EID/SID and DATA/RTR*/
#define SET_CAN_EID_MASK(x) (((x) << 3)|0b110)

#define CAN_FILTER_MODE_MASK 0U
#define CAN_FILTER_MODE_ID 1U
#define CAN_FILTER_SCALE_16_BITS 0U
#define CAN_FILTER_SCALE_32_BITS 1U
#define CAN_FILTER_FIFO_ASSIGN_0 0U
#define CAN_FILTER_FIFO_ASSIGN_1 1U

#define BTR_CAN_500KBAUD (CAN_BTR_SJW(0) | CAN_BTR_BRP(8) | \
			   CAN_BTR_TS1(8) | CAN_BTR_TS2(1))

#define BTR_CAN_1MBAUD   (CAN_BTR_SJW(0) | CAN_BTR_BRP(5) | \
			  CAN_BTR_TS1(5) | CAN_BTR_TS2(1))

#define BTR_CAN BTR_CAN_1MBAUD

#define CAN_FILTER_ID 0
#define CAN_FILTER_MASK 1

#define CAN_FILTER_TYPE CAN_FILTER_ID

/*
  Câbler une LED sur la broche C0


  ° connecter B6 (uart1_tx) sur PROBE+SERIAL Rx AVEC UN JUMPER
  ° connecter B7 (uart1_rx) sur PROBE+SERIAL Tx AVEC UN JUMPER
  ° connecter C0 sur led0
*/

/*
 * automatic wakeup, automatic recover
 * from abort mode.
 * See section 22.7.7 on the STM32 reference manual.
 */

static const CANConfig cancfg = {
  .mcr = CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
  .btr = BTR_CAN
};

static volatile uint32_t lastFrameIdx = 0;


static THD_WORKING_AREA(waBlinker, 304);	// declaration de la pile du thread blinker
noreturn static void blinker (void *arg)			// fonction d'entrée du thread blinker
{
  (void)arg;					// on dit au compilateur que "arg" n'est pas utilisé

  chRegSetThreadName("blinker");		// on nomme le thread
  const uint32_t role = (palReadLine(LINE_SD_DETECT) == PAL_LOW) ? ROLE_RECEIVER : ROLE_TRANSMITTER;
  
  while (true) {				
    palToggleLine(LINE_LED1);		
    chThdSleepMilliseconds(100*(2*role));
    DebugTrace("last rec frame idx = %ld", lastFrameIdx);
  }
}

static THD_WORKING_AREA(waCanTx, 304);	
noreturn static void canTx (void *arg)	
{
  (void)arg;				
  
  chRegSetThreadName("canTx");
  const uint32_t role = (palReadLine(LINE_SD_DETECT) == PAL_LOW) ? ROLE_RECEIVER : ROLE_TRANSMITTER;
  uint32_t count = 0;

  /* if (role == ROLE_RECEIVER) */
  /*   chThdSleep(TIME_INFINITE); */
  
  while (true) {			
    const CANTxFrame txmsg = {
			      .IDE = CAN_IDE_EXT,
			      .EID = 0x01234566+role,
			      .RTR = CAN_RTR_DATA,
			      .DLC = 8,
			      .data32[0] = role,
			      .data32[1] = count};
    
    msg_t status = canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg, TIME_MS2I(500));
    if (status != MSG_OK) {
      DebugTrace("cantx error %ld", status);
    } else {
      //DebugTrace("cantx OK");
      count++;
    }
    chThdSleepMicroseconds(200);
  }
}

static THD_WORKING_AREA(waCanRx, 304);	
noreturn static void canRx (void *arg)			
{
  (void)arg;					
  event_listener_t el;
  CANRxFrame rxmsg;

  chRegSetThreadName("canRx");		
  chEvtRegister(&CAND1.rxfull_event, &el, 0);
  const uint32_t role = (palReadLine(LINE_SD_DETECT) == PAL_LOW) ? ROLE_RECEIVER : ROLE_TRANSMITTER;
  
  /* while (true) { */
  /*   msg_t status; */
  /*       if (chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100)) == 0) */
  /*     continue; */
  /* 	do { */
  /* 	  status = canReceive(&CAND1, CAN_ANY_MAILBOX,  &rxmsg, TIME_IMMEDIATE); */
  /* 	  if (status == MSG_OK) { */
  /* 	    /\* Process message.*\/ */
  /* 	    DebugTrace("reception trame from role %s count=%ld", */
  /* 		       rxmsg.data32[0] == ROLE_RECEIVER ? "sdIn" : "sdLess", */
  /* 		       rxmsg.data32[1]); */
  /* 	  } else { */
  /* 	    // normal error when time immediate */
  /* 	    //DebugTrace("canrx error for role %ld %ld", role, status); */
  /* 	  } */
  /* 	} while (status == MSG_OK); */
  /* } */

  while (true) {
    const msg_t status = canReceive(&CAND1, CAN_ANY_MAILBOX,  &rxmsg, TIME_INFINITE);
    if (status == MSG_OK) {
      /* Process message.*/
      /* DebugTrace("reception trame from role %s count=%ld", */
      /* 		 rxmsg.data32[0] == ROLE_RECEIVER ? "sdIn" : "sdLess", */
      /* 		 rxmsg.data32[1]); */
      if ((rxmsg.data32[1] - lastFrameIdx) != 1) {
	DebugTrace("Frame lost");
      }
      lastFrameIdx = rxmsg.data32[1] ;
    } else {
      // normal error when time immediate
      DebugTrace("canrx error for role %ld %ld", role, status);
    }
  }
}


int main (void)
{

  halInit();
  chSysInit();
  initHeap();		// initialisation du "tas" pour permettre l'allocation mémoire dynamique 


#if (CAN_FILTER_TYPE == CAN_FILTER_MASK)
  CANFilter can_filter[] = {				
				{
				 .filter = 0,
				 .mode = CAN_FILTER_MODE_MASK,
				 .scale = CAN_FILTER_SCALE_32_BITS,
				 .assignment = CAN_FILTER_FIFO_ASSIGN_0,
				 .register1 = SET_CAN_EID_DATA(0x01234566+ROLE_RECEIVER),
				 .register2 = SET_CAN_EID_MASK(0x1FFFFFFF)
				},								
				
				
				{
				 .filter = 1,
				 .mode = CAN_FILTER_MODE_MASK,
				 .scale = CAN_FILTER_SCALE_32_BITS,
				 .assignment = CAN_FILTER_FIFO_ASSIGN_0,
				 .register1 = SET_CAN_EID_DATA(0x01234566+ROLE_TRANSMITTER),
				 .register2 = SET_CAN_EID_MASK(0x1FFFFFFF)
				}
  };
#elif (CAN_FILTER_TYPE == CAN_FILTER_ID)
 CANFilter can_filter[] = {				
				{
				 .filter = 0,
				 .mode = CAN_FILTER_MODE_ID,
				 .scale = CAN_FILTER_SCALE_32_BITS,
				 .assignment = CAN_FILTER_FIFO_ASSIGN_0, 
				 .register1 = SET_CAN_EID_DATA(0x01234566+ROLE_RECEIVER),
				 .register2 = SET_CAN_EID_DATA(0x01234566+ROLE_TRANSMITTER),
				}
 };
				
#else
#error "unknown CAN_FILTER_TYPE"
#endif
				
  // share filters between CAN1 and CAN2
  canSTM32SetFilters(&CAND1, STM32_CAN_MAX_FILTERS/2, ARRAY_LEN(can_filter), can_filter);


  canStart(&CAND1, &cancfg);

  consoleInit();	// initialisation des objets liés au shell
  chThdCreateStatic(waBlinker, sizeof(waBlinker), NORMALPRIO, &blinker, NULL); 
  chThdCreateStatic(waCanTx, sizeof(waCanTx), NORMALPRIO, &canTx, NULL); 
  chThdCreateStatic(waCanRx, sizeof(waCanRx), NORMALPRIO, &canRx, NULL); 

  // cette fonction en interne fait une boucle infinie, elle ne sort jamais
  // donc tout code situé après ne sera jamais exécuté.
  consoleLaunch();  // lancement du shell
  
  // main thread does nothing
  chThdSleep(TIME_INFINITE);
}


