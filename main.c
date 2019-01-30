#include <ch.h>
#include <hal.h>
#include <stdnoreturn.h>
#include "globalVar.h"
#include "stdutil.h"
#include "ttyConsole.h"


/*
  Câbler une LED sur la broche C0

  Câbler sur la carte de dev le chip convertisseur USB série :

  sonde RX sur B6 (utiliser un jumper)
  sonde TX sur B7 (utiliser un jumper)

 */


static THD_WORKING_AREA(waBlinker, 256);
static noreturn void blinker (void *arg)
{

  (void)arg;
  chRegSetThreadName("blinker");
  
  while (true) { 
    palToggleLine (LINE_C00_LED1); 	
    chThdSleepMilliseconds (500);
  }
}



int main(void) {

    /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */

  halInit();
  chSysInit();
  initHeap();

  consoleInit();
  chThdCreateStatic(waBlinker, sizeof(waBlinker), NORMALPRIO, blinker, NULL);

  consoleLaunch();  
  
  // main thread does nothing
  chThdSleep (TIME_INFINITE);
}

