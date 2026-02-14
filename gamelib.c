#include "gamelib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Statistiche base per il combattimento di un nemico. */
typedef struct {
    int hp;
    int attacco;
    int difesa;
} NemicoStats;

/*
 * Stato globale del gioco: elenco dei giocatori,
 * puntatori alle mappe e contatori.
 */
static Giocatore *giocatori[MAX_GIOCATORI];
static int num_giocatori = 0;
static int mappa_chiusa = 0;
static int rng_init = 0;
static int undici_virgola_cinque_usato = 0;

static Zona_mondoreale *prima_zona_mondoreale = NULL;
static Zona_soprasotto *prima_zona_soprasotto = NULL;

static char vincitori[3][NOME_MAX];
static int vincitori_count = 0;
static int partite_giocate = 0;

/*
 * Stampa effetto macchina da scrivere.
 * nanosec_delay controlla la velocità di stampa.
 */
void stampa_lenta(long nanosec_delay, const char *fmt, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = nanosec_delay;
    for (const char *p = buffer; *p; ++p) {
        fputc(*p, stdout);
        fflush(stdout);
        nanosleep(&ts, NULL);
    }
}

/* Inizializza il generatore di numeri casuali una sola volta. */
static void init_rng(void)
{
    if (!rng_init) {
        srand((unsigned)time(NULL));
        rng_init = 1;
    }
}

/*
 * Restituisce un intero casuale compreso tra min e max inclusi.
 */
static int randint(int min, int max)
{
    int range = max - min + 1;
    return min + (rand() % range);
}

/*
 * Svuota il buffer di input fino a newline/EOF.
 */
static void clear_input_line(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

/*
 * Legge un intero da tastiera con verifica:
 * ripete la richiesta finché il valore non è nel range.
 */

static int leggi_intero(const char *prompt, int min, int max)
{
    int valore;
    int letti;
    /* Loop del menu di configurazione: termina solo quando la mappa è chiusa. */
    do {
        stampa_lenta(15000000L, "%s", prompt);
        letti = scanf("%d", &valore);
        if (letti != 1) {
            stampa_lenta(15000000L, "Input non valido.\n");
            clear_input_line();
            continue;
        }
        if (valore < min || valore > max) {
            stampa_lenta(15000000L, "Valore fuori range (%d-%d).\n", min, max);
        }
        clear_input_line();
    } while (letti != 1 || valore < min || valore > max);
    return valore;
}

/*
 * Legge una stringa (nome giocatore) e rimuove il newline finale.
 * Se l'utente inserisce una riga vuota usa un nome di default.
 */

static void leggi_stringa(const char *prompt, char *dest, size_t max_len)
{
    stampa_lenta(15000000L, "%s", prompt);
    if (fgets(dest, (int)max_len, stdin) == NULL) {
        dest[0] = '\0';
        return;
    }
    dest[strcspn(dest, "\n")] = '\0';
    if (dest[0] == '\0') {
        strncpy(dest, "Giocatore", max_len);
        dest[max_len - 1] = '\0';
    }
}

static const char *nome_tipo_zona(Tipo_zona tipo)
{
    switch (tipo) {
    case bosco:
        return "bosco";
    case scuola:
        return "scuola";
    case laboratorio:
        return "laboratorio";
    case caverna:
        return "caverna";
    case strada:
        return "strada";
    case giardino:
        return "giardino";
    case supermercato:
        return "supermercato";
    case centrale_elettrica:
        return "centrale_elettrica";
    case deposito_abbandonato:
        return "deposito_abbandonato";
    case stazione_polizia:
        return "stazione_polizia";
    default:
        return "sconosciuto";
    }
}

static const char *nome_nemico(Tipo_nemico nemico)
{
    switch (nemico) {
    case nessun_nemico:
        return "nessun_nemico";
    case billi:
        return "billi";
    case democane:
        return "democane";
    case demotorzone:
        return "demotorzone";
    default:
        return "sconosciuto";
    }
}

static const char *nome_oggetto(Tipo_oggetto oggetto)
{
    switch (oggetto) {
    case nessun_oggetto:
        return "nessun_oggetto";
    case bicicletta:
        return "bicicletta";
    case maglietta_fuocoinferno:
        return "maglietta_fuocoinferno";
    case bussola:
        return "bussola";
    case schitarrata_metallica:
        return "schitarrata_metallica";
    default:
        return "sconosciuto";
    }
}


/*
 * Restituisce le statistiche base del nemico scelto.
 */
static NemicoStats stats_nemico(Tipo_nemico nemico)
{
    NemicoStats s;
    switch (nemico) {
    case billi:
        s.hp = 12;
        s.attacco = 8;
        s.difesa = 8;
        break;
    case democane:
        s.hp = 16;
        s.attacco = 10;
        s.difesa = 10;
        break;
    case demotorzone:
        s.hp = 24;
        s.attacco = 14;
        s.difesa = 12;
        break;
    case nessun_nemico:
    default:
        s.hp = 0;
        s.attacco = 0;
        s.difesa = 0;
        break;
    }
    return s;
}

/* Alloca e inizializza una zona del Mondo Reale. */
static Zona_mondoreale *crea_zona_mr(Tipo_zona tipo, Tipo_nemico nemico, Tipo_oggetto oggetto)
{
    Zona_mondoreale *z = (Zona_mondoreale *)malloc(sizeof(Zona_mondoreale));
    if (!z) {
        return NULL;
    }
    z->tipo = tipo;
    z->nemico = nemico;
    z->oggetto = oggetto;
    z->avanti = NULL;
    z->indietro = NULL;
    z->link_soprasotto = NULL;
    return z;
}

/* Alloca e inizializza una zona del Soprasotto. */
static Zona_soprasotto *crea_zona_ss(Tipo_zona tipo, Tipo_nemico nemico)
{
    Zona_soprasotto *z = (Zona_soprasotto *)malloc(sizeof(Zona_soprasotto));
    if (!z) {
        return NULL;
    }
    z->tipo = tipo;
    z->nemico = nemico;
    z->avanti = NULL;
    z->indietro = NULL;
    z->link_mondoreale = NULL;
    return z;
}

/*
 * Libera la memoria di tutte le mappe
 * e ripristina i puntatori globali.
 */

static void libera_mappa(void)
{
    Zona_mondoreale *cur_mr = prima_zona_mondoreale;
    while (cur_mr) {
        Zona_mondoreale *next = cur_mr->avanti;
        free(cur_mr);
        cur_mr = next;
    }
    prima_zona_mondoreale = NULL;

    Zona_soprasotto *cur_ss = prima_zona_soprasotto;
    while (cur_ss) {
        Zona_soprasotto *next = cur_ss->avanti;
        free(cur_ss);
        cur_ss = next;
    }
    prima_zona_soprasotto = NULL;
}

/*
 * Libera la memoria dei giocatori e azzera lo stato.
 */
static void libera_giocatori(void)
{
    for (int i = 0; i < MAX_GIOCATORI; i++) {
        free(giocatori[i]);
        giocatori[i] = NULL;
    }
    num_giocatori = 0;
    undici_virgola_cinque_usato = 0;
}

/*
 * Conta quante zone esistono nella mappa del Mondo Reale.
 */
static int conta_zone_mr(void)
{
    int count = 0;
    Zona_mondoreale *cur = prima_zona_mondoreale;
    while (cur) {
        count++;
        cur = cur->avanti;
    }
    return count;
}

/*
 * Conta quante zone del Soprasotto hanno il demotorzone.
 */
static int conta_demotorzone_ss(void)
{
    int count = 0;
    Zona_soprasotto *cur = prima_zona_soprasotto;
    while (cur) {
        if (cur->nemico == demotorzone) {
            count++;
        }
        cur = cur->avanti;
    }
    return count;
}

/*
 * Estrae casualmente un tipo di zona.
 */
static Tipo_zona random_tipo_zona(void)
{
    return (Tipo_zona)randint(0, stazione_polizia);
}

/*
 * Estrae casualmente un nemico per il Mondo Reale.
 */
static Tipo_nemico random_nemico_mr(void)
{
    int r = randint(1, 100);
    if (r <= 40) {
        return nessun_nemico;
    }
    if (r <= 70) {
        return democane;
    }
    return billi;
}

/*
 * Estrae casualmente un nemico per il Soprasotto.
 */
static Tipo_nemico random_nemico_ss(void)
{
    int r = randint(1, 100);
    if (r <= 45) {
        return nessun_nemico;
    }
    if (r <= 80) {
        return democane;
    }
    return demotorzone;
}

/*
 * Estrae casualmente un oggetto per il Mondo Reale.
 */
static Tipo_oggetto random_oggetto(void)
{
    int r = randint(1, 100);
    if (r <= 40) {
        return nessun_oggetto;
    }
    return (Tipo_oggetto)randint(bicicletta, schitarrata_metallica);
}

/*
 * Aggancia in coda una coppia di zone mondoreale/soprasotto mantenendo i link.
 */
static void append_coppia(Zona_mondoreale *mr, Zona_soprasotto *ss)
{
    mr->link_soprasotto = ss;
    ss->link_mondoreale = mr;

    if (!prima_zona_mondoreale) {
        prima_zona_mondoreale = mr;
    } else {
        Zona_mondoreale *cur = prima_zona_mondoreale;
        while (cur->avanti) {
            cur = cur->avanti;
        }
        cur->avanti = mr;
        mr->indietro = cur;
    }

    if (!prima_zona_soprasotto) {
        prima_zona_soprasotto = ss;
    } else {
        Zona_soprasotto *cur = prima_zona_soprasotto;
        while (cur->avanti) {
            cur = cur->avanti;
        }
        cur->avanti = ss;
        ss->indietro = cur;
    }
}

/*
 * Crea da zero la mappa di gioco con 15 zone per mondo,
 * assegnando tipo, nemico e oggetto in modo casuale e
 * garantendo la presenza di un solo demotorzone nel Soprasotto.
 */

static void genera_mappa(void)
{
    libera_mappa();
    for (int i = 0; i < 15; i++) {
        Tipo_zona tipo = random_tipo_zona();
        Tipo_nemico nemico_mr = random_nemico_mr();
        Tipo_oggetto oggetto = random_oggetto();
        Tipo_nemico nemico_ss = random_nemico_ss();

        Zona_mondoreale *mr = crea_zona_mr(tipo, nemico_mr, oggetto);
        Zona_soprasotto *ss = crea_zona_ss(tipo, nemico_ss);
        if (!mr || !ss) {
            free(mr);
            free(ss);
            stampa_lenta(15000000L, "Errore di allocazione durante la generazione della mappa.\n");
            libera_mappa();
            return;
        }
        append_coppia(mr, ss);
    }

    if (conta_demotorzone_ss() == 0) {
        int pos = randint(1, 15);
        Zona_soprasotto *cur = prima_zona_soprasotto;
        for (int i = 1; i < pos && cur; i++) {
            cur = cur->avanti;
        }
        if (cur) {
            cur->nemico = demotorzone;
        }
    }
    while (conta_demotorzone_ss() > 1) {
        Zona_soprasotto *cur = prima_zona_soprasotto;
        int trovato = 0;
        while (cur) {
            if (cur->nemico == demotorzone) {
                if (!trovato) {
                    trovato = 1;
                } else {
                    cur->nemico = democane;
                }
            }
            cur = cur->avanti;
        }
    }
    stampa_lenta(15000000L, "Mappa generata con 15 zone per ciascun mondo.\n");
}

/*
 * Chiede all'utente il nemico del Mondo Reale per una zona.
 */
static int scegli_nemico_mr(void)
{
    stampa_lenta(15000000L, "Nemici Mondo Reale: 0) nessun_nemico 1) billi 2) democane\n");
    return leggi_intero("Scelta nemico MR: ", 0, 2);
}

/*
 * Chiede all'utente il nemico del Soprasotto.
 */
static int scegli_nemico_ss(int demotorzone_disponibile)
{
    if (demotorzone_disponibile) {
        stampa_lenta(15000000L, "Nemici Soprasotto: 0) nessun_nemico 2) democane 3) demotorzone\n");
        int scelta;
        do {
            scelta = leggi_intero("Scelta nemico SS: ", 0, 3);
        } while (scelta == 1);
        return scelta;
    }
    stampa_lenta(15000000L, "Nemici Soprasotto: 0) nessun_nemico 2) democane\n");
    int scelta;
    do {
        scelta = leggi_intero("Scelta nemico SS: ", 0, 2);
    } while (scelta == 1);
    return scelta;
}

/*
 * Chiede all'utente l'oggetto da associare a una zona.
 */
static int scegli_oggetto(void)
{
    stampa_lenta(15000000L, "Oggetti: 0) nessun_oggetto 1) bicicletta 2) maglietta_fuocoinferno 3) bussola 4) schitarrata_metallica\n");
    return leggi_intero("Scelta oggetto: ", 0, 4);
}

/*
 * Inserisce una nuova coppia di zone in posizione scelta,
 * mantenendo la corrispondenza tra Mondo Reale e Soprasotto.
 */

static void inserisci_zona(void)
{
    int len = conta_zone_mr();
    int pos = leggi_intero("Posizione di inserimento (1..len+1): ", 1, len + 1);
    Tipo_zona tipo = random_tipo_zona();

    int demotorzone_disponibile = conta_demotorzone_ss() == 0;
    Tipo_nemico nemico_mr = (Tipo_nemico)scegli_nemico_mr();
    Tipo_nemico nemico_ss = (Tipo_nemico)scegli_nemico_ss(demotorzone_disponibile);
    Tipo_oggetto oggetto = (Tipo_oggetto)scegli_oggetto();

    Zona_mondoreale *mr = crea_zona_mr(tipo, nemico_mr, oggetto);
    Zona_soprasotto *ss = crea_zona_ss(tipo, nemico_ss);
    if (!mr || !ss) {
        free(mr);
        free(ss);
        stampa_lenta(15000000L, "Errore di allocazione durante l'inserimento.\n");
        return;
    }

    mr->link_soprasotto = ss;
    ss->link_mondoreale = mr;

    if (pos == 1) {
        mr->avanti = prima_zona_mondoreale;
        if (prima_zona_mondoreale) {
            prima_zona_mondoreale->indietro = mr;
        }
        prima_zona_mondoreale = mr;

        ss->avanti = prima_zona_soprasotto;
        if (prima_zona_soprasotto) {
            prima_zona_soprasotto->indietro = ss;
        }
        prima_zona_soprasotto = ss;
    } else {
        Zona_mondoreale *cur_mr = prima_zona_mondoreale;
        Zona_soprasotto *cur_ss = prima_zona_soprasotto;
        for (int i = 1; i < pos - 1 && cur_mr && cur_ss; i++) {
            cur_mr = cur_mr->avanti;
            cur_ss = cur_ss->avanti;
        }
        mr->avanti = cur_mr->avanti;
        if (cur_mr->avanti) {
            cur_mr->avanti->indietro = mr;
        }
        cur_mr->avanti = mr;
        mr->indietro = cur_mr;

        ss->avanti = cur_ss->avanti;
        if (cur_ss->avanti) {
            cur_ss->avanti->indietro = ss;
        }
        cur_ss->avanti = ss;
        ss->indietro = cur_ss;
    }
    stampa_lenta(15000000L, "Zona inserita in posizione %d.\n", pos);
}

/*
 * Rimuove una coppia di zone dalla posizione indicata,
 * aggiornando i puntatori della lista e le posizioni dei giocatori.
 */

static void cancella_zona(void)
{
    int len = conta_zone_mr();
    if (len == 0) {
        stampa_lenta(15000000L, "Non ci sono zone da cancellare.\n");
        return;
    }
    int pos = leggi_intero("Posizione da cancellare (1..len): ", 1, len);

    Zona_mondoreale *cur_mr = prima_zona_mondoreale;
    Zona_soprasotto *cur_ss = prima_zona_soprasotto;
    for (int i = 1; i < pos && cur_mr && cur_ss; i++) {
        cur_mr = cur_mr->avanti;
        cur_ss = cur_ss->avanti;
    }
    if (!cur_mr || !cur_ss) {
        stampa_lenta(15000000L, "Posizione non valida.\n");
        return;
    }

    if (cur_mr->indietro) {
        cur_mr->indietro->avanti = cur_mr->avanti;
    } else {
        prima_zona_mondoreale = cur_mr->avanti;
    }
    if (cur_mr->avanti) {
        cur_mr->avanti->indietro = cur_mr->indietro;
    }

    if (cur_ss->indietro) {
        cur_ss->indietro->avanti = cur_ss->avanti;
    } else {
        prima_zona_soprasotto = cur_ss->avanti;
    }
    if (cur_ss->avanti) {
        cur_ss->avanti->indietro = cur_ss->indietro;
    }

    for (int i = 0; i < MAX_GIOCATORI; i++) {
        if (!giocatori[i]) {
            continue;
        }
        if (giocatori[i]->pos_mondoreale == cur_mr) {
            giocatori[i]->pos_mondoreale = prima_zona_mondoreale;
        }
        if (giocatori[i]->pos_soprasotto == cur_ss) {
            giocatori[i]->pos_soprasotto = prima_zona_soprasotto;
        }
    }

    free(cur_mr);
    free(cur_ss);
    stampa_lenta(15000000L, "Zona cancellata.\n");
}

/*
 * Stampa i campi di una zona del Mondo Reale.
 */
static void stampa_zona_mr(Zona_mondoreale *z, int idx)
{
    if (!z) {
        return;
    }
    stampa_lenta(15000000L, "[%d] tipo=%s nemico=%s oggetto=%s\n", idx, nome_tipo_zona(z->tipo),
           nome_nemico(z->nemico), nome_oggetto(z->oggetto));
}

/*
 * Stampa i campi di una zona del Soprasotto.
 */
static void stampa_zona_ss(Zona_soprasotto *z, int idx)
{
    if (!z) {
        return;
    }
    stampa_lenta(15000000L, "[%d] tipo=%s nemico=%s\n", idx, nome_tipo_zona(z->tipo),
           nome_nemico(z->nemico));
}

/*
 * Stampa tutte le zone di una delle due mappe.
 */
static void stampa_mappa(void)
{
    int scelta = leggi_intero("Stampa mappa: 0) Mondo Reale 1) Soprasotto: ", 0, 1);
    if (scelta == 0) {
        Zona_mondoreale *cur = prima_zona_mondoreale;
        int idx = 1;
        while (cur) {
            stampa_zona_mr(cur, idx++);
            cur = cur->avanti;
        }
    } else {
        Zona_soprasotto *cur = prima_zona_soprasotto;
        int idx = 1;
        while (cur) {
            stampa_zona_ss(cur, idx++);
            cur = cur->avanti;
        }
    }
}

/*
 * Stampa una zona specifica del Mondo Reale e la sua speculare.
 */
static void stampa_zona_scelta(void)
{
    int len = conta_zone_mr();
    if (len == 0) {
        stampa_lenta(15000000L, "Mappa vuota.\n");
        return;
    }
    int pos = leggi_intero("Posizione zona (1..len): ", 1, len);
    Zona_mondoreale *cur_mr = prima_zona_mondoreale;
    Zona_soprasotto *cur_ss = prima_zona_soprasotto;
    for (int i = 1; i < pos && cur_mr && cur_ss; i++) {
        cur_mr = cur_mr->avanti;
        cur_ss = cur_ss->avanti;
    }
    stampa_lenta(15000000L, "Mondo Reale: ");
    stampa_zona_mr(cur_mr, pos);
    stampa_lenta(15000000L, "Soprasotto: ");
    stampa_zona_ss(cur_ss, pos);
}

/*
 * Chiude la fase di creazione della mappa verificando i vincoli:
 * almeno 15 zone e un solo demotorzone nel Soprasotto.
 */

static void chiudi_mappa(void)
{
    int len = conta_zone_mr();
    int demotorzone_count = conta_demotorzone_ss();
    if (len < 15) {
        stampa_lenta(15000000L, "Servono almeno 15 zone per chiudere la mappa.\n");
        return;
    }
    if (demotorzone_count != 1) {
        stampa_lenta(15000000L, "Deve esserci esattamente un demotorzone nel Soprasotto.\n");
        return;
    }
    mappa_chiusa = 1;
    stampa_lenta(15000000L, "Mappa chiusa correttamente.\n");
}

/*
 * Menu interattivo per costruire la mappa; resta attivo
 * finché non si chiama con successo chiudi_mappa().
 */

static void menu_imposta_mappa(void)
{
    int scelta;
    do {
        stampa_lenta(15000000L, "\n--- Menu Impostazione Mappa ---\n");
        stampa_lenta(15000000L, "1) genera_mappa\n");
        stampa_lenta(15000000L, "2) inserisci_zona\n");
        stampa_lenta(15000000L, "3) cancella_zona\n");
        stampa_lenta(15000000L, "4) stampa_mappa\n");
        stampa_lenta(15000000L, "5) stampa_zona\n");
        stampa_lenta(15000000L, "6) chiudi_mappa\n");
        scelta = leggi_intero("Scelta: ", 1, 6);
        switch (scelta) {
        case 1:
            genera_mappa();
            break;
        case 2:
            inserisci_zona();
            break;
        case 3:
            cancella_zona();
            break;
        case 4:
            stampa_mappa();
            break;
        case 5:
            stampa_zona_scelta();
            break;
        case 6:
            chiudi_mappa();
            break;
        default:
            break;
        }
    } while (!mappa_chiusa);
}

/*
 * Imposta le statistiche iniziali del giocatore (d20),
 * applica eventuali modifiche e prepara lo zaino vuoto.
 */

static void inizializza_giocatore(Giocatore *g)
{
    int dado = randint(1, 20);
    g->attacco_psichico = dado;
    g->difesa_psichica = dado;
    g->fortuna = dado;

    stampa_lenta(15000000L, "Valori iniziali (d20=%d): attacco=%d difesa=%d fortuna=%d\n",
           dado, g->attacco_psichico, g->difesa_psichica, g->fortuna);

    stampa_lenta(15000000L, "Scegli modifica: 1) +3 attacco, -3 difesa 2) +3 difesa, -3 attacco 3) nessuna\n");
    int scelta = leggi_intero("Scelta: ", 1, 3);
    if (scelta == 1) {
        g->attacco_psichico += 3;
        g->difesa_psichica -= 3;
    } else if (scelta == 2) {
        g->difesa_psichica += 3;
        g->attacco_psichico -= 3;
    }

    if (!undici_virgola_cinque_usato) {
        int scelta_speciale = leggi_intero("Vuoi diventare UndiciVirgolaCinque? 1) si 2) no: ", 1, 2);
        if (scelta_speciale == 1) {
            g->attacco_psichico += 4;
            g->difesa_psichica += 4;
            g->fortuna -= 7;
            snprintf(g->nome, NOME_MAX, "UndiciVirgolaCinque_%s", g->nome);
            undici_virgola_cinque_usato = 1;
        }
    }

    if (g->attacco_psichico < 1) {
        g->attacco_psichico = 1;
    }
    if (g->difesa_psichica < 1) {
        g->difesa_psichica = 1;
    }
    if (g->fortuna < 1) {
        g->fortuna = 1;
    }
    if (g->attacco_psichico > 20) {
        g->attacco_psichico = 20;
    }
    if (g->difesa_psichica > 20) {
        g->difesa_psichica = 20;
    }
    if (g->fortuna > 20) {
        g->fortuna = 20;
    }

    for (int i = 0; i < ZAINO_MAX; i++) {
        g->zaino[i] = nessun_oggetto;
    }
}

/*
 * Fase di setup del gioco: crea i giocatori, azzera stato precedente
 * e avvia il menu di creazione mappa.
 */

void imposta_gioco(void)
{
    init_rng();

    libera_mappa();
    libera_giocatori();
    mappa_chiusa = 0;

    num_giocatori = leggi_intero("Numero giocatori (1-4): ", 1, 4);
    for (int i = 0; i < num_giocatori; i++) {
        giocatori[i] = (Giocatore *)malloc(sizeof(Giocatore));
        if (!giocatori[i]) {
            stampa_lenta(15000000L, "Errore di allocazione giocatore.\n");
            libera_giocatori();
            return;
        }
        memset(giocatori[i], 0, sizeof(Giocatore));
        leggi_stringa("Nome giocatore: ", giocatori[i]->nome, NOME_MAX);
        inizializza_giocatore(giocatori[i]);
    }

    for (int i = num_giocatori; i < MAX_GIOCATORI; i++) {
        giocatori[i] = NULL;
    }

    /* Fino a qui sono stati creati i giocatori; ora si costruisce la mappa. */
    menu_imposta_mappa();

    stampa_lenta(15000000L, "Impostazione gioco completata.\n");
}

/*
 * Verifica se non ci sono più giocatori vivi.
 */
static int tutti_morti(void)
{
    for (int i = 0; i < MAX_GIOCATORI; i++) {
        if (giocatori[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * Salva il nome del vincitore nelle ultime tre partite.
 */
static void registra_vincitore(const char *nome)
{
    if (vincitori_count < 3) {
        strncpy(vincitori[vincitori_count], nome, NOME_MAX);
        vincitori[vincitori_count][NOME_MAX - 1] = '\0';
        vincitori_count++;
    } else {
        for (int i = 0; i < 2; i++) {
            strncpy(vincitori[i], vincitori[i + 1], NOME_MAX);
        }
        strncpy(vincitori[2], nome, NOME_MAX);
        vincitori[2][NOME_MAX - 1] = '\0';
    }
    partite_giocate++;
}

/*
 * Stampa le informazioni complete di un giocatore.
 */
static void stampa_giocatore(Giocatore *g)
{
    if (!g) {
        return;
    }
    stampa_lenta(15000000L, "Giocatore: %s\n", g->nome);
    stampa_lenta(15000000L, "Mondo: %s\n", g->mondo == 0 ? "Mondo Reale" : "Soprasotto");
    stampa_lenta(15000000L, "Attacco: %d Difesa: %d Fortuna: %d\n", g->attacco_psichico, g->difesa_psichica, g->fortuna);
    stampa_lenta(15000000L, "Zaino: ");
    for (int i = 0; i < ZAINO_MAX; i++) {
        stampa_lenta(15000000L, "%s", nome_oggetto(g->zaino[i]));
        if (i < ZAINO_MAX - 1) {
            stampa_lenta(15000000L, ", ");
        }
    }
    stampa_lenta(15000000L, "\n");
}

/*
 * Stampa la zona in cui si trova il giocatore.
 */
static void stampa_zona_corrente(Giocatore *g)
{
    if (g->mondo == 0) {
        stampa_lenta(15000000L, "Zona Mondo Reale: ");
        stampa_zona_mr(g->pos_mondoreale, 0);
    } else {
        stampa_lenta(15000000L, "Zona Soprasotto: ");
        stampa_zona_ss(g->pos_soprasotto, 0);
    }
}

/*
 * Raccoglie un oggetto dalla zona, se possibile.
 */
static int raccogli_oggetto(Giocatore *g)
{
    if (g->mondo != 0) {
        stampa_lenta(15000000L, "Nel Soprasotto non ci sono oggetti.\n");
        return 0;
    }
    Zona_mondoreale *z = g->pos_mondoreale;
    if (z->nemico != nessun_nemico) {
        stampa_lenta(15000000L, "Prima devi sconfiggere il nemico.\n");
        return 0;
    }
    if (z->oggetto == nessun_oggetto) {
        stampa_lenta(15000000L, "Nessun oggetto da raccogliere.\n");
        return 0;
    }
    for (int i = 0; i < ZAINO_MAX; i++) {
        if (g->zaino[i] == nessun_oggetto) {
            g->zaino[i] = z->oggetto;
            stampa_lenta(15000000L, "Oggetto raccolto: %s\n", nome_oggetto(z->oggetto));
            z->oggetto = nessun_oggetto;
            return 1;
        }
    }
    stampa_lenta(15000000L, "Zaino pieno.\n");
    return 0;
}

/*
 * Applica l'effetto di un oggetto a giocatore/nemico.
 */
static int usa_oggetto_effetto(Giocatore *g, Tipo_oggetto oggetto, int *hp_nemico)
{
    switch (oggetto) {
    case bicicletta:
        g->fortuna += 2;
        if (g->fortuna > 20) {
            g->fortuna = 20;
        }
        stampa_lenta(15000000L, "Fortuna aumentata a %d.\n", g->fortuna);
        break;
    case maglietta_fuocoinferno:
        g->attacco_psichico += 3;
        if (g->attacco_psichico > 20) {
            g->attacco_psichico = 20;
        }
        stampa_lenta(15000000L, "Attacco aumentato a %d.\n", g->attacco_psichico);
        break;
    case bussola:
        g->difesa_psichica += 2;
        if (g->difesa_psichica > 20) {
            g->difesa_psichica = 20;
        }
        stampa_lenta(15000000L, "Difesa aumentata a %d.\n", g->difesa_psichica);
        break;
    case schitarrata_metallica:
        if (hp_nemico) {
            *hp_nemico -= 5;
            stampa_lenta(15000000L, "Schitarrata metallica! Danni al nemico (-5).\n");
        } else {
            g->attacco_psichico += 1;
            g->difesa_psichica += 1;
            if (g->attacco_psichico > 20) {
                g->attacco_psichico = 20;
            }
            if (g->difesa_psichica > 20) {
                g->difesa_psichica = 20;
            }
            stampa_lenta(15000000L, "Attacco e difesa aumentati.\n");
        }
        break;
    default:
        return 0;
    }
    return 1;
}

/*
 * Permette al giocatore di scegliere un oggetto dallo zaino
 * e applicarne l'effetto, consumandolo.
 */

static int utilizza_oggetto(Giocatore *g, int *hp_nemico)
{
    int indice = -1;
    for (int i = 0; i < ZAINO_MAX; i++) {
        if (g->zaino[i] != nessun_oggetto) {
            indice = i;
            break;
        }
    }
    if (indice == -1) {
        stampa_lenta(15000000L, "Zaino vuoto.\n");
        return 0;
    }

    stampa_lenta(15000000L, "Scegli oggetto:\n");
    for (int i = 0; i < ZAINO_MAX; i++) {
        stampa_lenta(15000000L, "%d) %s\n", i + 1, nome_oggetto(g->zaino[i]));
    }
    int scelta = leggi_intero("Scelta: ", 1, ZAINO_MAX);
    Tipo_oggetto oggetto = g->zaino[scelta - 1];
    if (oggetto == nessun_oggetto) {
        stampa_lenta(15000000L, "Nessun oggetto nello slot.\n");
        return 0;
    }
    if (usa_oggetto_effetto(g, oggetto, hp_nemico)) {
        g->zaino[scelta - 1] = nessun_oggetto;
        return 1;
    }
    return 0;
}

/*
 * Gestisce un combattimento a turni contro il nemico della stanza.
 * Usa attacco/difesa/fortuna del giocatore e aggiorna lo stato del nemico.
 */

static int combatti(Giocatore *g, Tipo_nemico *nemico, int *vittoria_demotorzone)
{
    if (*nemico == nessun_nemico) {
        stampa_lenta(15000000L, "Nessun nemico presente.\n");
        return 1;
    }

    NemicoStats stats = stats_nemico(*nemico);
    int hp_nemico = stats.hp;
    int hp_giocatore = 10 + g->difesa_psichica;

    /* Inizia il ciclo principale del combattimento a turni. */
    stampa_lenta(15000000L, "Combattimento contro %s!\n", nome_nemico(*nemico));
    while (hp_nemico > 0 && hp_giocatore > 0) {
        stampa_lenta(15000000L, "HP giocatore: %d | HP nemico: %d\n", hp_giocatore, hp_nemico);
        stampa_lenta(15000000L, "1) Attacca 2) Usa oggetto\n");
        int scelta = leggi_intero("Scelta: ", 1, 2);
        if (scelta == 2) {
            utilizza_oggetto(g, &hp_nemico);
        } else {
            int tiro_g = randint(1, 20) + g->attacco_psichico;
            int tiro_n = randint(1, 20) + stats.difesa;
            if (tiro_g >= tiro_n) {
                int danno = 2 + g->attacco_psichico / 4;
                int fortuna = randint(1, 20);
                if (fortuna <= g->fortuna) {
                    danno += 2;
                    stampa_lenta(15000000L, "Colpo fortunato! Danni aumentati.\n");
                }
                hp_nemico -= danno;
                stampa_lenta(15000000L, "Colpito! Danni %d.\n", danno);
            } else {
                stampa_lenta(15000000L, "Attacco respinto.\n");
            }
        }

        if (hp_nemico <= 0) {
            break;
        }

        int tiro_n = randint(1, 20) + stats.attacco;
        int tiro_g = randint(1, 20) + g->difesa_psichica;
        if (tiro_n > tiro_g) {
            int danno = 2 + stats.attacco / 5;
            int fortuna = randint(1, 20);
            if (fortuna <= g->fortuna) {
                danno -= 2;
                if (danno < 1) {
                    danno = 1;
                }
                stampa_lenta(15000000L, "La fortuna ti protegge! Danni ridotti.\n");
            }
            hp_giocatore -= danno;
            stampa_lenta(15000000L, "Il nemico colpisce! Danni %d.\n", danno);
        } else {
            stampa_lenta(15000000L, "Hai evitato l'attacco.\n");
        }
    }

    if (hp_giocatore <= 0) {
        stampa_lenta(15000000L, "Il giocatore %s è morto.\n", g->nome);
        for (int i = 0; i < MAX_GIOCATORI; i++) {
            if (giocatori[i] == g) {
                free(giocatori[i]);
                giocatori[i] = NULL;
                break;
            }
        }
        return 0;
    }

    stampa_lenta(15000000L, "Nemico sconfitto!\n");
    if (*nemico == demotorzone) {
        *vittoria_demotorzone = 1;
    }
    int scompare = randint(1, 100) <= 50;
    if (scompare) {
        *nemico = nessun_nemico;
        stampa_lenta(15000000L, "Il nemico è scomparso dalla zona.\n");
    }
    return 1;
}

/*
 * Sposta il giocatore alla zona successiva nella mappa corrente,
 * se non ha già avanzato nel turno e dopo l'eventuale combattimento.
 */

static int avanza(Giocatore *g, int *vittoria_demotorzone, int *ha_avanzato)
{
    if (*ha_avanzato) {
        stampa_lenta(15000000L, "Hai già avanzato in questo turno.\n");
        return 0;
    }
    if (g->mondo == 0) {
        if (!combatti(g, &g->pos_mondoreale->nemico, vittoria_demotorzone)) {
            return 0;
        }
        if (g->pos_mondoreale->avanti) {
            g->pos_mondoreale = g->pos_mondoreale->avanti;
            g->pos_soprasotto = g->pos_mondoreale->link_soprasotto;
            *ha_avanzato = 1;
            stampa_lenta(15000000L, "Avanzato nel Mondo Reale.\n");
            return 1;
        }
    } else {
        if (!combatti(g, &g->pos_soprasotto->nemico, vittoria_demotorzone)) {
            return 0;
        }
        if (g->pos_soprasotto->avanti) {
            g->pos_soprasotto = g->pos_soprasotto->avanti;
            g->pos_mondoreale = g->pos_soprasotto->link_mondoreale;
            *ha_avanzato = 1;
            stampa_lenta(15000000L, "Avanzato nel Soprasotto.\n");
            return 1;
        }
    }
    stampa_lenta(15000000L, "Non puoi avanzare oltre.\n");
    return 0;
}

/*
 * Sposta il giocatore alla zona precedente nella mappa corrente,
 * rispettando le stesse regole di avanzamento e combattimento.
 */

static int indietreggia(Giocatore *g, int *vittoria_demotorzone, int *ha_avanzato)
{
    if (*ha_avanzato) {
        stampa_lenta(15000000L, "Hai già avanzato in questo turno.\n");
        return 0;
    }
    if (g->mondo == 0) {
        if (!combatti(g, &g->pos_mondoreale->nemico, vittoria_demotorzone)) {
            return 0;
        }
        if (g->pos_mondoreale->indietro) {
            g->pos_mondoreale = g->pos_mondoreale->indietro;
            g->pos_soprasotto = g->pos_mondoreale->link_soprasotto;
            *ha_avanzato = 1;
            stampa_lenta(15000000L, "Indietreggiato nel Mondo Reale.\n");
            return 1;
        }
    } else {
        if (!combatti(g, &g->pos_soprasotto->nemico, vittoria_demotorzone)) {
            return 0;
        }
        if (g->pos_soprasotto->indietro) {
            g->pos_soprasotto = g->pos_soprasotto->indietro;
            g->pos_mondoreale = g->pos_soprasotto->link_mondoreale;
            *ha_avanzato = 1;
            stampa_lenta(15000000L, "Indietreggiato nel Soprasotto.\n");
            return 1;
        }
    }
    stampa_lenta(15000000L, "Non puoi indietreggiare oltre.\n");
    return 0;
}

/*
 * Consente il passaggio tra Mondo Reale e Soprasotto:
 * nel Mondo Reale richiede un tiro fortuna e non permette
 * il cambio se si è già avanzato nel turno.
 */

static int cambia_mondo(Giocatore *g, int *vittoria_demotorzone, int *ha_avanzato)
{
    if (g->mondo == 0) {
        if (*ha_avanzato) {
            stampa_lenta(15000000L, "Hai già avanzato in questo turno.\n");
            return 0;
        }
        if (!combatti(g, &g->pos_mondoreale->nemico, vittoria_demotorzone)) {
            return 0;
        }
        int tiro = randint(1, 20);
        if (tiro >= g->fortuna) {
            stampa_lenta(15000000L, "Tentativo fallito (tiro %d, fortuna %d).\n", tiro, g->fortuna);
            return 0;
        }
        g->mondo = 1;
        g->pos_soprasotto = g->pos_mondoreale->link_soprasotto;
        *ha_avanzato = 1;
        stampa_lenta(15000000L, "Sei entrato nel Soprasotto.\n");
        return 1;
    }

    g->mondo = 0;
    g->pos_mondoreale = g->pos_soprasotto->link_mondoreale;
    stampa_lenta(15000000L, "Sei tornato nel Mondo Reale.\n");
    return 1;
}

/*
 * Gestisce il menu delle azioni del singolo giocatore
 * per tutta la durata del suo turno.
 */

static void turno_giocatore(Giocatore *g, int *vittoria_demotorzone)
{
    int ha_avanzato = 0;
    int finito = 0;
    while (!finito && g) {
        stampa_lenta(15000000L, "\n--- Turno di %s ---\n", g->nome);
        stampa_lenta(15000000L, "1) avanza\n");
        stampa_lenta(15000000L, "2) indietreggia\n");
        stampa_lenta(15000000L, "3) cambia_mondo\n");
        stampa_lenta(15000000L, "4) combatti\n");
        stampa_lenta(15000000L, "5) stampa_giocatore\n");
        stampa_lenta(15000000L, "6) stampa_zona\n");
        stampa_lenta(15000000L, "7) raccogli_oggetto\n");
        stampa_lenta(15000000L, "8) utilizza_oggetto\n");
        stampa_lenta(15000000L, "9) passa\n");
        int scelta = leggi_intero("Scelta: ", 1, 9);
        switch (scelta) {
        case 1:
            avanza(g, vittoria_demotorzone, &ha_avanzato);
            break;
        case 2:
            indietreggia(g, vittoria_demotorzone, &ha_avanzato);
            break;
        case 3:
            cambia_mondo(g, vittoria_demotorzone, &ha_avanzato);
            break;
        case 4:
            if (g->mondo == 0) {
                combatti(g, &g->pos_mondoreale->nemico, vittoria_demotorzone);
            } else {
                combatti(g, &g->pos_soprasotto->nemico, vittoria_demotorzone);
            }
            break;
        case 5:
            stampa_giocatore(g);
            break;
        case 6:
            stampa_zona_corrente(g);
            break;
        case 7:
            raccogli_oggetto(g);
            break;
        case 8:
            utilizza_oggetto(g, NULL);
            break;
        case 9:
            finito = 1;
            break;
        default:
            break;
        }
        if (*vittoria_demotorzone) {
            finito = 1;
        }
        if (!g) {
            finito = 1;
        }
    }
}

/*
 * Posiziona i giocatori nella prima zona di entrambe le mappe.
 */
static void imposta_posizioni_iniziali(void)
{
    for (int i = 0; i < MAX_GIOCATORI; i++) {
        if (!giocatori[i]) {
            continue;
        }
        giocatori[i]->mondo = 0;
        giocatori[i]->pos_mondoreale = prima_zona_mondoreale;
        giocatori[i]->pos_soprasotto = prima_zona_soprasotto;
    }
}

/*
 * Avvia la partita, assegna le posizioni iniziali,
 * alterna i turni in ordine casuale e determina la vittoria.
 */

void gioca(void)
{
    if (!mappa_chiusa || !prima_zona_mondoreale || num_giocatori == 0) {
        stampa_lenta(15000000L, "Gioco non impostato correttamente.\n");
        return;
    }

    imposta_posizioni_iniziali();
    int vittoria = 0;
    char vincitore[NOME_MAX] = "";

    /* A questo punto partita avviata: si alternano i turni finché non c'è vittoria o tutti morti. */
    while (!vittoria && !tutti_morti()) {
        int indici[MAX_GIOCATORI];
        int count = 0;
        for (int i = 0; i < MAX_GIOCATORI; i++) {
            if (giocatori[i]) {
                indici[count++] = i;
            }
        }
        for (int i = count - 1; i > 0; i--) {
            int j = randint(0, i);
            int tmp = indici[i];
            indici[i] = indici[j];
            indici[j] = tmp;
        }

        for (int i = 0; i < count; i++) {
            Giocatore *g = giocatori[indici[i]];
            if (!g) {
                continue;
            }
            int vittoria_demotorzone = 0;
            turno_giocatore(g, &vittoria_demotorzone);
            if (vittoria_demotorzone) {
                vittoria = 1;
                strncpy(vincitore, g->nome, NOME_MAX);
                vincitore[NOME_MAX - 1] = '\0';
                break;
            }
            if (tutti_morti()) {
                break;
            }
        }
    }

    if (vittoria) {
        stampa_lenta(15000000L, "Il vincitore e' %s!\n", vincitore);
        registra_vincitore(vincitore);
    } else {
        stampa_lenta(15000000L, "Tutti i giocatori sono morti. Fine partita.\n");
        registra_vincitore("Nessuno");
    }
}

/*
 * Termina il gioco e libera tutte le risorse allocate.
 */
/*
 * Termina il gioco e libera le risorse allocate.
 */
void termina_gioco(void)
{
    stampa_lenta(15000000L, "Termine del gioco. Arrivederci!\n");
    libera_mappa();
    libera_giocatori();
    mappa_chiusa = 0;
}

/*
 * Mostra informazioni sull'autore e i risultati delle ultime partite.
 */
/*
 * Mostra autore e statistiche delle partite precedenti.
 */
void crediti(void)
{
    stampa_lenta(15000000L, "\n--- Crediti ---\n");
    stampa_lenta(15000000L, "Creatore: Inserire Nome Cognome\n");
    stampa_lenta(15000000L, "Partite giocate: %d\n", partite_giocate);
    stampa_lenta(15000000L, "Vincitori ultime tre partite:\n");
    if (vincitori_count == 0) {
        stampa_lenta(15000000L, "- Nessuno\n");
    } else {
        for (int i = 0; i < vincitori_count; i++) {
            stampa_lenta(15000000L, "- %s\n", vincitori[i]);
        }
    }
}
