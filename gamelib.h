#ifndef GAMELIB_H
#define GAMELIB_H

#include <stddef.h>

#define MAX_GIOCATORI 4
#define ZAINO_MAX 3
#define NOME_MAX 64

/* Tipi di zona disponibili */
typedef enum {
    bosco,
    scuola,
    laboratorio,
    caverna,
    strada,
    giardino,
    supermercato,
    centrale_elettrica,
    deposito_abbandonato,
    stazione_polizia
} Tipo_zona;

/* Tipi di nemico che possono apparire */
typedef enum {
    nessun_nemico,
    billi,
    democane,
    demotorzone
} Tipo_nemico;

/* Tipi di oggetto che possono essere trovati/gestiti nello zaino. */
typedef enum {
    nessun_oggetto,
    bicicletta,
    maglietta_fuocoinferno,
    bussola,
    schitarrata_metallica
} Tipo_oggetto;

/*
 * Zona del Mondo Reale, 
 * contiene il possibile oggetto e il link alla zona speculare.
 */
typedef struct Zona_mondoreale {
    Tipo_zona tipo;
    Tipo_nemico nemico;
    Tipo_oggetto oggetto;
    struct Zona_mondoreale *avanti;
    struct Zona_mondoreale *indietro;
    struct Zona_soprasotto *link_soprasotto;
} Zona_mondoreale;

/*
 * Zona del Soprasotto,
 * non contiene oggetti ma mantiene il link alla zona del Mondo Reale.
 */
typedef struct Zona_soprasotto {
    Tipo_zona tipo;
    Tipo_nemico nemico;
    struct Zona_soprasotto *avanti;
    struct Zona_soprasotto *indietro;
    struct Zona_mondoreale *link_mondoreale;
} Zona_soprasotto;

/*
 * Struttura che rappresenta un giocatore, con statistiche,
 * posizione nei due mondi e inventario limitato.
 */
typedef struct {
    char nome[NOME_MAX];
    int mondo;
    Zona_mondoreale *pos_mondoreale;
    Zona_soprasotto *pos_soprasotto;
    int attacco_psichico;
    int difesa_psichica;
    int fortuna;
    Tipo_oggetto zaino[ZAINO_MAX];
} Giocatore;

/* Funzioni pubbliche */
void imposta_gioco(void);
void gioca(void);
void termina_gioco(void);
void crediti(void);
void stampa_lenta(long nanosec_delay, const char *fmt, ...);

#endif
