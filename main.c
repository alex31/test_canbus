/*
  Nom(s), prénom(s) du ou des élèves : 

  QUESTION 1 : influence de la suppression du tableau prendDeLaPlaceSurLaPile ?

 */
#include <ch.h>
#include <hal.h>
#include "stdutil.h"		// necessaire pour initHeap
#include "ttyConsole.h"		// fichier d'entête du shell
#include "stdnoreturn.h"


#define ROLE_SATURATE 0x2
#define ROLE_ABORT_THEN_SEND_HIGHPRIO 0x4

#define CAN_SATURATE_EID	1000U

/*
  ° Pas de filtrage pour simplifier
  ° un thread sature le bus en envoyant en boucle un msg id 1000 decimal 
  ° l'autre thread permet par le shell 
	* d'envoyer des messages (id et data en param)
	* d'annuler un messaghe (mailbox en param)

  ° en reception on affiche un msg que si l'eid est != de 1000
  ° l'idée est de tester l'API abort du driver can
 */

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

#define CAN_REC(canp) (((canp)->can->ESR & CAN_ESR_REC_Msk) >> CAN_ESR_REC_Pos)
#define CAN_TEC(canp) (((canp)->can->ESR & CAN_ESR_TEC_Msk) >> CAN_ESR_TEC_Pos)



#define BTR_CAN_500KBAUD (CAN_BTR_SJW(0) | CAN_BTR_BRP(8) | \
			   CAN_BTR_TS1(8) | CAN_BTR_TS2(1))

#define BTR_CAN_1MBAUD   (CAN_BTR_SJW(0) | CAN_BTR_BRP(5) | \
			  CAN_BTR_TS1(5) | CAN_BTR_TS2(1))

#define CAN_TX_FIFO_MODE  CAN_MCR_TXFP
#define CAN_TX_PRIORITY_MODE  0U


#define CAN_FILTER_TYPE			CAN_FILTER_ID
#define BTR_CAN				BTR_CAN_1MBAUD
#define CAN_TX_SCHEDULE			CAN_TX_FIFO_MODE
//#define CAN_TX_SCHEDULE		CAN_TX_PRIORITY_MODE

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
				 .mcr = CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_TX_SCHEDULE,
				 .btr = BTR_CAN
};

static volatile uint32_t lastFrameIdx = 0;


static THD_WORKING_AREA(waBlinker, 304);	// declaration de la pile du thread blinker
noreturn static void blinker (void *arg)			// fonction d'entrée du thread blinker
{
  (void)arg;					// on dit au compilateur que "arg" n'est pas utilisé

  const uint32_t role = (palReadLine(LINE_SD_DETECT) == PAL_LOW) ?
    ROLE_SATURATE :
    ROLE_ABORT_THEN_SEND_HIGHPRIO;

  chRegSetThreadName(role == ROLE_SATURATE ? "blink saturate" : "blink try abort");

  while (true) {				
    palToggleLine(LINE_LED1);		
    chThdSleepMilliseconds(100*(2*role));
    /* DebugTrace("last rec frame idx = %ld tec=%ld rec=%ld", */
    /* 	       lastFrameIdx, */
    /* 	       CAN_TEC(&CAND1), */
    /* 	       CAN_REC(&CAND1) */
    /* 	       ); */
  }
}

static THD_WORKING_AREA(waCanTx, 304);	
noreturn static void canTx (void *arg)	
{
  (void)arg;				
  
  chRegSetThreadName("canTx");
  const uint32_t role = (palReadLine(LINE_SD_DETECT) == PAL_LOW) ?
    ROLE_SATURATE :
    ROLE_ABORT_THEN_SEND_HIGHPRIO;
  uint32_t count = 0;

   if (role == ROLE_ABORT_THEN_SEND_HIGHPRIO) 
    chThdSleep(TIME_INFINITE);
  
  while (true) {			
    const CANTxFrame txmsg = {
			      .IDE = CAN_IDE_EXT,
			      .EID = CAN_SATURATE_EID,
			      .RTR = CAN_RTR_DATA,
			      .DLC = 4,
			      .data32[0] = count,
			      .data32[1] = 0};
    
    msg_t status = canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg, TIME_INFINITE);
    if (status != MSG_OK) {
      DebugTrace("cantx error %ld", status);
    } else {
      //DebugTrace("cantx OK");
      count++;
    }
  }
}

static THD_WORKING_AREA(waCanRx, 304);	
noreturn static void canRx (void *arg)			
{
  (void)arg;					
  CANRxFrame rxmsg;

  chRegSetThreadName("canRx");		

  const uint32_t role = (palReadLine(LINE_SD_DETECT) == PAL_LOW) ?
    ROLE_SATURATE :
    ROLE_ABORT_THEN_SEND_HIGHPRIO;

 
  while (true) {
    const msg_t status = canReceive(&CAND1, CAN_ANY_MAILBOX,  &rxmsg, TIME_INFINITE);
    if (status == MSG_OK) {
      /* Process message.*/
      if (rxmsg.EID != CAN_SATURATE_EID) {
	DebugTrace("reception trame from eid %d d[0]=%ld",
		   rxmsg.EID,
		   rxmsg.data32[0]
		   );
      }
    } else {
      // normal error when time immediate, timout error if not time_infinite
      DebugTrace("canrx error for role %ld %ld", role, status);
    }
  }
}

int main (void)
{

  halInit();
  chSysInit();
  initHeap();		// initialisation du "tas" pour permettre l'allocation mémoire dynamique 

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


