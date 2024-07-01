#include "x_func.h" //FUNZIONI DI XERRORI USATE A LEZIONE
#include "getopt.h" //LETTURA ARGOMENTI PASSATI INLINE
#include "read.h" //LETTURA E CREAZIONE GRAFO
#include "math.h" //PER USARE fabs()
#include <stdatomic.h>

#define BATCH_SIZE 1000 
#define BufSize 1000 
#define index __LINE__,__FILE__ 

/*  OPZIONI DEFAULT */
#define K 3 //Nodi Stampati
#define M 100 //Massimo numero iterazoini
#define D 0.9 //Damping factor
#define E 1.0e-7 //Errore massimo
#define T 3 //Numero Thread ausiliari

/* STRUTTURE PER THREAD AUSILIARI PER PAGERANK */

typedef struct{ //valori numerici condivisi tra i thread 
    double *st, *st_next, *err, d, first_term;
    grafo *g;
    double *x, *xnext;
    int t;
}shareData;

typedef struct { //struttura per la comunicazione tra i thread 
    atomic_int *pos;
    pthread_mutex_t *mutex_buf, *mutex_next;
    sem_t *sem_start, *sem_end;
    bool continua;
    shareData *support; //valori numerici (uguali)
}threadData;

typedef struct{ //struttura per handler di sigusr1
    int *iter;
    double *rank;
    int dim;
    int *exit;
    bool started;
}dati_sign;

/* PROTOTIPI FUNZIONI */
void *thr_signal(void *arg);
void *aux(void *arg);
double *pagerank(grafo *g, double d, double eps, int maxiter, int taux, int *numiter);
void stampa_max(double *vet, int dim, int k, FILE *stream);

int main(int argc, char *argv[]){
    /* CONFIGURAZIONE MASCHERA */
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGQUIT); //escludo SIGQUIT dalla maschera
    pthread_sigmask(SIG_BLOCK, &mask, NULL); //applico la maschera al main

    /* LETTURA ARGOMENTI INLINE */
    int opt, k = K, m = M, t = T;
    double d = D, e = E;
    char *infile = NULL;

    while((opt = getopt(argc, argv, "k:m:d:e:t:")) != -1){
        switch(opt){
            case 'k':
                k = atoi(optarg);
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case 'd':
                d = atoi(optarg);
                break;
            case 'e':
                e = atof(optarg);
                break;
            case 't':
                t = atoi(optarg);
                break;
            case '?':
                termina("Errore: Argomento mancante / Operazione non Riconosciuta\n");
            default:
                termina("Errore: Opzione Non Riconosciuta\n");
        }
    }

    /* CONTROLLO LA PRESENZA DEL PARAMETRO OBBLIGATORIO */
    if(optind < argc) //optind indica il prossimo argomento non-opzionale da processare
        infile = argv[optind]; 
    else 
        termina("Errore: File Non Specificato\n"); 
    
    /* STRUMENTI PER SINCRONIZZAZIONE PROD/CONS*/
    coppia *buffer = malloc(BufSize * sizeof(coppia));
    int p_index = 0, c_index = 0;

    pthread_mutex_t mutex_buf = PTHREAD_MUTEX_INITIALIZER;
    sem_t sem_free_slot, sem_data_item;
    xsem_init(&sem_free_slot, 0, BufSize, index);
    xsem_init(&sem_data_item, 0, 0, index);

    grafo *Grafo = malloc(sizeof(grafo));
    if(Grafo == NULL) termina("Errore: Allocazione Grafo Fallita\n");

    thread_cons dati_consumatori[t];
    pthread_t id_consumatori[t];

    /* CREAZIONE THREAD CONSUMATORI */
    for(int i=0; i<t; i++){
        dati_consumatori[i] = (thread_cons){
            .buffer = buffer, 
            .g = Grafo, 
            .ind = &c_index,
            .mutex_buf = &mutex_buf, 
            .sem_data_item = &sem_data_item,
            .sem_free_slot = &sem_free_slot
        };
        xpthread_create(&id_consumatori[i],NULL,&consumatori,&dati_consumatori[i],index);
    }

    /* AVVIO PRODUTTORE (MAIN)*/
    FILE *f = fopen(infile, "rt");
    if(f==NULL) termina("Errore: Apertura Infile Errata\n");

    ssize_t res;
    size_t len;
    char *linea = NULL;
   
    while(true){
        res = getline(&linea, &len, f);
        if(linea[0] != '%') {
            break;
        } //righe di commento terminate
    }
        
        
    int r, c, n, i, j;
    res = sscanf(linea, "%d %d %d", &r, &c, &n); 
    if(res != 3) termina("Errore: Prima riga errata\n");

    init_grafo(Grafo, r);

    /* LETTURA DEGLI ARCHI */
    while(getline(&linea, &len, f) != -1){
        res = sscanf(linea, "%d %d", &i, &j);
        if(res != 2) termina("Errore: Riga file errata\n");        
        /* INSERISCO NEL BUFFER UN NUOVO ARCO */
        xsem_wait(&sem_free_slot, index);
        buffer[p_index % BufSize].a = i;
        buffer[p_index % BufSize].b = j;
        p_index++;
        xsem_post(&sem_data_item, index);
    }

    /* COMUNICO LA FINE DELLA LETTURA DEGLI ARCHI */
    for(int i=0; i<t; i++){
        xsem_wait(&sem_free_slot, index);
        buffer[p_index % BufSize].a = -1;
        buffer[p_index % BufSize].b = -1;
        p_index++;
        xsem_post(&sem_data_item, index);
    }

    /* FINE THREAD E CHIUSURA FILE */
    for(int i=0; i<t; i++)
        xpthread_join(id_consumatori[i],NULL,index);
    fclose(f);
    free(linea);
    /* MODIFICO LA GRANDEZZA DEI G->IN[J].DIMENSIONE CON IL NUMERO CORRETTO*/
    check_size_grafo(Grafo);
  
    /* CHIAMATA E STAMPA RISULTATO DEL PAGERANK */
    int iter = 0;
    double *rank = pagerank(Grafo, d, e, m, t, &iter);

    printf("Number of nodes: %d\n", Grafo->N);
    int sum = 0;
    for(int i=0; i<Grafo->N; i++)
        if(Grafo->out[i]==0)
            sum += 1;
    printf("Number of dead-end nodes: %d\n",sum);
    printf("Number of valid arcs: %d\n", n-Grafo->invalid_arc);
    
    if(iter > m)
        printf("Did not converge after %d iterations\n",m);
    else
        printf("Converged after %d iterations\n",iter);

    double tot = 0.0;
    for(int i=0; i<Grafo->N; i++)
        tot += rank[i];
    
    printf("Sum of ranks: %5.4f   (should be %g)\n",tot,tot);
    printf("Top %d nodes:\n",k);
    stampa_max(rank, Grafo->N, k, stdout);

    /* GARBAGE */
    free(rank);
    free(buffer);
    free_grafo(Grafo);
    xpthread_mutex_destroy(&mutex_buf, index);
    sem_destroy(&sem_data_item);
    sem_destroy(&sem_free_slot);
    return 0;
}

/* FUNZIONE DEI THREAD AUSILIARI (PER PAGERANK)*/
void *aux(void *arg){
    threadData *set = (threadData*)arg; //da usare per sincronizzazione
    shareData *data = (shareData*)set->support; //da usare per calcoli
    
    double sumy;
    int get, start, end;
    int batch_size = BATCH_SIZE > data->g->N ? 1 : BATCH_SIZE;
    // batch_size = 1; per parallelizzazione teorica
    while(true){
        xsem_wait(set->sem_start, index); //attendo la nuova iterazione (T+1)
        if(set->continua == false) break; //fine calcolo pagerank

        double pers_st_next = 0.0, pers_err = 0.0;
        while(true){ // calcolo degli indici relativi al tempo T
            get = atomic_fetch_add(set->pos, batch_size);
            start = get;
            end = (get+batch_size) < data->g->N ? (get+batch_size) : data->g->N;

            if(start >= data->g->N){ 
                break;
            }

            /* calcolo delle componenti XNEXT da start a end */
            for(int j=start; j<end; j++){
                sumy = 0.0;
                inmap temp = data->g->in[j];
                
                /* calcolo della componente XNEXT[J] al tempo T */
                for(int i=0; i<temp.elementi; i++)
                    sumy += (double) (data->x[temp.vettore[i]] / data->g->out[temp.vettore[i]]);
                
                data->xnext[j] = data->first_term + (data->d*sumy) + *data->st;

                /* calcolo di err ed st locali */
                if(data->g->out[j] == 0) pers_st_next += data->xnext[j];
                pers_err += fabs(data->xnext[j]-data->x[j]);
            }
        }

        /* i thread a turno incrementano err e st_next relativi al tempo T*/
        xpthread_mutex_lock(set->mutex_next, index);
        *(data->err) += pers_err;
        *(data->st_next) += pers_st_next;
        xpthread_mutex_unlock(set->mutex_next, index);
       
        /* fine lavoro al tempo T */
        xsem_post(set->sem_end, index);
    }
    return (void*)0;
}
double *pagerank(grafo *g, double d, double eps, int maxiter, int taux, int *numiter){
    /* INIZIALIZZAZIONE ARRAY E VARIABILI */
    double *x = malloc(sizeof(double) * g->N);
    double *xnext = malloc(sizeof(double) * g->N);
    if( x==NULL || xnext==NULL)
        termina("Errore: Allocazione Vettori Fallita\n");

    double first_term = (1.0 - d) / (double)g->N;
    int iter = 0,  exit = 0;
    double st = 0.0, st_next = 0.0, err = 0.0;
    atomic_int aux_pos = ATOMIC_VAR_INIT(0);

    /* DATI E THREAD PER GESTIONE SEGNALI  */  
    pthread_t id_signal_thr;
    dati_sign set = (dati_sign){
        .dim = g->N, .iter = &iter,
        .rank = xnext, .exit = &exit
    };
    xpthread_create(&id_signal_thr, NULL, &thr_signal, &set, index);

    /* INIZIALIZZAZIONE ST, X */
    for(int i=0; i<g->N; i++){
        x[i] = 1.0 / (double)g->N;
        if(g->out[i] == 0) st += x[i];
    }
    st = (double)(d / (double)g->N) * st;

    /* DATI PER SINCRONIZZAZIONE CON I THREAD AUSILIARI*/
    pthread_mutex_t mutex_buf = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_next = PTHREAD_MUTEX_INITIALIZER;
    
    sem_t sem_start, sem_end;
    xsem_init(&sem_start, 0, 0, index);
    xsem_init(&sem_end, 0, 0, index);

    pthread_t id_aux[taux];
    threadData data_aux[taux];
    
    /* struttura per dati numerici da comunicare ai thread aux */
    shareData support = (shareData){
        .d = d,
        .first_term = first_term,
        .st = &st,
        .err = &err,
        .st_next = &st_next,
        .x = x,
        .xnext = xnext,
        .g = g,
        .t = taux,
    };

    /* creazione strutture e thread ausiliari */
    for(int i=0; i<taux; i++){
        data_aux[i] = (threadData){ //struttura con tutti i dati necessari
            .sem_end = &sem_end,
            .sem_start = &sem_start,
            .continua = true,
            .mutex_buf = &mutex_buf,
            .mutex_next = &mutex_next,
            .pos = &aux_pos,
            .support = &support
        };
        xpthread_create(&id_aux[i],NULL, &aux, &data_aux[i],index);
    }
    
    /* INIZIO DEL CALCOLO DEL PAGERANK */
    while(iter++ < maxiter){
        err = 0.0;
        st_next = 0.0;
        atomic_init(&aux_pos, 0); //variabile su cui i thread ausiliari faranno lock/unlock        

        /* al tempo t c'è nuovo lavoro per i thread aux */
        for(int l=0; l<taux; l++) 
            xsem_post(&sem_start, index);
        
        /* attendo che i thread aux finiscano il lavoro */
        for(int l=0; l<taux; l++) 
            xsem_wait(&sem_end, index);
        
        /* aggiornamento dati e calcolo della convergenza */
        memcpy(x, xnext, sizeof(double)*g->N);
        st = (double)(d / (double)(g->N)) * st_next;
        if(err < eps) break;    
    }

    /* comunico la fine del pagerank */
    for(int l=0; l<taux; l++){ 
        data_aux[l].continua = false;
        xsem_post(&sem_start, index);
    }
    
    for(int l=0; l<taux; l++)
        pthread_join(id_aux[l],NULL);

    /* faccio terminare il thread gestore dei segnali */
    exit = 1; 
    pthread_kill(id_signal_thr, SIGUSR1);
    pthread_join(id_signal_thr, NULL);

    /* garbage */
    pthread_mutex_destroy(&mutex_buf);
    pthread_mutex_destroy(&mutex_next);
    sem_destroy(&sem_start);
    sem_destroy(&sem_end);
    
    free(xnext);

    /* risultato pagerank */
    *numiter = iter;
    return x;
}
/* THREAD PER GESTIRE I SEGNALI */
void *thr_signal(void *arg){
    dati_sign *d = (dati_sign*)arg;
    sigset_t mask;
    sigemptyset(&mask); 
    sigaddset(&mask, SIGUSR1); //trattengo SIGUSR1 per gestirlo

    int s;
    while(*d->exit == 0){
        int e = sigwait(&mask,&s);
        if(e != 0) xtermina("Errore: sigwait\n",index);
        if(s == SIGUSR1 && *d->exit == 0){
            fprintf(stderr,"Tempo (%d): ", *d->iter);
            stampa_max(d->rank, d->dim, 1, stderr);
        }
    }
    return (void*)0;
}
/* FUNZIONE PER STAMPARE SU STREAM K MASSIMI IN VET */
void stampa_max(double *vet, int dim, int k, FILE *stream){
    double prec = 2; 
    while(k > 0){
        double max  = 0.0;
        int ind = -1;
        for(int i=0; i<dim; i++){
            if(vet[i] > max && vet[i] < prec){ /*confronto col precedente massimo*/
                max = vet[i];
                ind=i;
            }
        }
        fprintf(stream,"  %d %f\n",ind,vet[ind]);
        prec = max;
        k--;
    }
}
