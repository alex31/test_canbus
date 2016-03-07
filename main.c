#include <ch.h>
#include <hal.h>
#include <stdnoreturn.h>
#include "globalVar.h"
#include "stdutil.h"
#include "ttyConsole.h"


/*
  Câbler une LED sur la broche C0

Câbler sur la carte de dev le chip convertisseur USB série :

ftdi RX sur B6 (utiliser un jumper)
ftdi TX sur B7 (utiliser un jumper)

compiler, flasher le projet
vérifier que la LED clignote

lancer le terminal dans eclipse
tester le shell
<tab> à l'invite de commande donne la liste des commandes possibles, une fois les premiers 
caractères tapés, <tab> génère l'autocompletion
tester les commandes « info » et « threads » (la colonne frestk donne la place libre sur la pile)

éditer le fichier main.c
après  la ligne chRegSetThreadName("blinker"); commenter la ligne
 int prendDeLaPlaceSurLaPile[40] __attribute__((unused));

compiler, flasher, relancer la commande thread, voir l'influence sur la place prise 
sur la pile privée du thread blinker au niveau de la colonne  frestk


Dans le fichier ttyConsole.c, il y a une variable globale, statique
(elle n'est pas visible en dehors de son unité de compilation), et
const : elle sera read only en mémoire flash et n'occupera pas
inutilement de place dans la ram. Cette variable, le tableau commands
est le point d'entrée des commandes. C'est un tableau de structures
clefs,valeurs, avec comme clef les mots clefs du shell, et comme
valeur un pointeur sur une fonction qui sera appelée lorsque ce mot
clef est tapé.  

En prenant exemple sur les commandes existantes, créer
une fonction qui permette de changer la fréquence de clignotement de
la LED, ajouter cette fonction dans le tableau commands, et tester.


Conseils : il va falloir que le module ttyConsole.c et le module
main.c partagent une valeur: la vitesse de clignotement de la LED.

 Le plus simple est d'utiliser une variable globale qui sera déclarée dans
globalVar.h et définie dans globalVar.c : dans le .h => extern
volatile int blinkPeriod ; // déclaration dand le .c => volatile int
blinkPeriod  = 500; // définition Explication du mot clef volatile :
une variable volatile est une variable sur laquelle aucune
optimisation de compilation n'est appliquée. Le mot-clé existe en C,
C++, C# et Java. le préfixe volatile est notamment utilisé quand la
variable d'une tâche peut être modifiée par une autre tâche ou une
routine d'interruption (ISR) de manière concurrente.  Dans tous les TP
qui suivront, les variables partagées entre plusieurs threads, ou
entre un thread et une routine d'interruption, devront être déclarées
avec le mot clef volatile

 */


static THD_WORKING_AREA(waBlinker, 256);
static noreturn void blinker (void *arg)
{

  (void)arg;
  chRegSetThreadName("blinker");
  int prendDeLaPlaceSurLaPile[40] __attribute__((unused)); 
  
  while (TRUE) { 
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
  while (TRUE) {
    chThdSleepSeconds (1);
  }
}

void  port_halt (void)
{
  while (1) ;
}

void generalKernelErrorCB (void)
{
  port_halt ();
}
