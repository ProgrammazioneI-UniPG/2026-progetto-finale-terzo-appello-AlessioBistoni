#include <stdio.h>

#include "gamelib.h"

/*
 * Punto di ingresso del programma: mostra il menu principale,
 * valida l'input dell'utente e richiama le funzioni della libreria di gioco.
 */
int main(void)
{
    int scelta;
    const char *banner =
        "  ,- _~.                     -_-/    ,                          \n"
        " (' /|                      (_ /    ||          _               \n"
        "((  ||    /'\\\\  _-_,  _-_  (_ --_  =||= ,._-_  < \\, \\\\/\\\\  _-_  \n"
        "((  ||   || || ||_.  || \\\\   --_ )  ||   ||    /-|| || || || \\\\ \n"
        " ( / |   || ||  ~ || ||/    _/  ))  ||   ||   (( || || || ||/   \n"
        "  -____- \\\\,/  ,-_-  \\\\,/  (_-_-    \\\\,  \\\\,   \\\\/\\\\ \\\\ \\\\ \\\\,/  \n"
        "                                                                 \n";

    stampa_lenta(5000000L, "%s", banner);

    /* Ciclo principale del menu: si esce solo scegliendo "termina gioco". */
    do {
        stampa_lenta(15000000L, "\n--- Menu ---\n");
        stampa_lenta(15000000L, "1) imposta gioco\n");
        stampa_lenta(15000000L, "2) gioca\n");
        stampa_lenta(15000000L, "3) termina gioco\n");
        stampa_lenta(15000000L, "4) crediti\n");
        stampa_lenta(15000000L, "Scelta: ");
        if (scanf("%d", &scelta) != 1) {
            stampa_lenta(15000000L, "Comando non valido.\n");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {
            }
            scelta = 0;
            continue;
        }
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }

        switch (scelta) {
        case 1:
            imposta_gioco();
            break;
        case 2:
            gioca();
            break;
        case 3:
            termina_gioco();
            break;
        case 4:
            crediti();
            break;
        default:
            stampa_lenta(15000000L, "Comando non valido.\n");
            break;
        }
    } while (scelta != 3);

    return 0;
}
