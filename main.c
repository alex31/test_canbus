#include <ch.h>
#include <hal.h>
#include <ctype.h>

/*
° connecter B6 (uart1_tx) sur ftdi rx AVEC UN JUMPER
  
° connecter B7 (uart1_rx) sur ftdi tx AVEC UN JUMPER
  
° connecter C0 sur led0
  
° ouvrir le projet devbm4_tp02_echo : le programme fourni écoute les caractères qui arrivent    
  sur la liaison série, et les renvoie à l’émetteur.
  
° ouvrir le fichier source main.c, et trouver la vitesse  de la liaison série.
  dans eclipse, perspective debug ouvrir le terminal, et le connecter en appliquant les 
  paramètres trouvés dans la question au dessus. 

  + (on accède au paramètres en cliquant sur la bouton qui en bas à droite qui représente un terminal)
  + le port est /dev/bmp_tty sous linux
  + une fois configuré, le terminal dans eclipse envoie les caractères tapés au clavier sur la 
    liaison série associée au convertisseur usb-serie, et affiche les caractères 
    en provenance de ce même convertisseur.
 
° make project
° debug project

° modifier le programme pour renvoyer les lettres en majuscules
  + vous pouvez utiliser la fonction de la librairie standard  toupper

° commenter les 2 lignes palSetLineMode qui effectuent l'initialisation dynamique
  et editer le fichier board.h pour à la place configurer statiquement la liaison série
  
° comment faire pour rendre le clignotement de la led indépendant de l’écho de caractères ?
  
° rendre le clignotement de la LED indépendant de l’écho de caractères

*/


static const SerialConfig ftdiConfig =  {
  .speed = 115200,
  .cr1   = 0,			 // pas de parité
  .cr2   = USART_CR2_STOP1_BITS, // 1 bit de stop
  .cr3   = 0			
};


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

  // initialisation dynamique
  palSetLineMode (LINE_B06_UART1_TX, PAL_MODE_ALTERNATE(7));
  palSetLineMode (LINE_B07_UART1_RX, PAL_MODE_ALTERNATE(7));
  sdStart(&SD1, &ftdiConfig);

  /*
    la led branchée sur GPIOC_PIN00 change d'état à la reception d'un caractère
  */
  while (TRUE) {
    palToggleLine (LINE_C00_LED1); 	
    char input = sdGet (&SD1);
    sdPut (&SD1, input);
  }
}

void  port_halt (void)
{
   while (1) {
   }
}

void generalKernelErrorCB (void)
{
  port_halt ();
}
