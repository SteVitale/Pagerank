#include "read.h"

/* DIMENSIONE DEFAULT DI G->IN[J]->VETTORE*/
#define start_size 100

/*  BODY DEI THREAD CONSUMATORI */
void *consumatori(void *arg){
    thread_cons *data = (thread_cons*)arg;

    coppia temp;
    while(true){
        xsem_wait(data->sem_data_item, index);
        xpthread_mutex_lock(data->mutex_buf, index);

        temp = data->buffer[*data->ind % BufSize];
        *data->ind +=1;

        xpthread_mutex_unlock(data->mutex_buf, index);
        xsem_post(data->sem_free_slot, index);
        
        if(temp.a == -1 && temp.b == -1) {
            break;
        }

        xpthread_mutex_lock(&data->g->mutex[temp.b-1], index);
        add_grafo(data->g, temp.a, temp.b);
        xpthread_mutex_unlock(&data->g->mutex[temp.b-1], index);
    }

    return (void*)0;
}

/*  FUNZIONI PER GESTIRE IL GRAFO   */ 
void init_grafo(grafo *g, int dim){ //INIZIALIZZAZIONE DI TUTTI GLI ELEMENTI DEL GRAFO
    assert(g != NULL && dim > 0); 
    g->N = dim;
    g->out = (atomic_int*)calloc(dim, sizeof(atomic_int));
    g->in = (inmap*)malloc(dim * sizeof(inmap));
    g->mutex = malloc(sizeof(pthread_mutex_t) * dim);
    
    /* sanity check*/
    if(g->in == NULL || g->out == NULL || g->mutex == NULL)
        termina("Errore: Allocazione Grafo fallita");
        
    for(int i=0; i<dim; i++){
        g->in[i].vettore = NULL;
        g->in[i].dimensione = start_size;
        g->in[i].elementi = 0;
        g->mutex[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    }
    g->invalid_arc = 0;
}
void add_grafo(grafo *g, int i, int j){
    assert(i>0 && j>0 && g != NULL);
    if(i == j){
        g->invalid_arc += 1;
        return;
    }
    if(g->in[j-1].vettore == NULL)
        g->in[j-1].vettore = malloc(sizeof(int) * start_size);

    if(g->in[j-1].vettore == NULL)
        xtermina("Errore: Vettore IN[J] nullo\n", index);
    
    /* controllo se i è già presente in IN[j] */
    for(int ind = 0; ind < g->in[j-1].elementi; ind++){
        if(i-1 == g->in[j-1].vettore[ind]){
            g->invalid_arc +=1;
            return;
        }
    }

    /* gestisco il caso in cui ho riempito IN[j] */
    if(g->in[j-1].elementi == g->in[j-1].dimensione){
        g->in[j-1].dimensione *= 2;
        g->in[j-1].vettore = realloc(g->in[j-1].vettore, sizeof(int)*g->in[j-1].dimensione);
        if(g->in[j-1].vettore == NULL)
            xtermina("Errore: Realloc fallita\n", index);
    }

    // IN(j) = {i:(i,j) in G}
    g->in[j-1].vettore[g->in[j-1].elementi++] = i-1;

    // OUT(j) = |{i:(j,i) in G}|
    atomic_fetch_add(&(g->out[i-1]),1);
}
void check_size_grafo(grafo *g){ // elimino gli spazi allocati inutilmente in tutti gli inmap
    for(int i=0; i<g->N; i++){
        if(g->in[i].elementi != 0){
            g->in[i].vettore = realloc(g->in[i].vettore, sizeof(int)*g->in[i].elementi);
            if(g->in[i].vettore == NULL)
                xtermina("Errore: reimpostazione IN[j] fallita\n", index);
        }
    }
}
void free_grafo(grafo *g){ //libero la memoria allocata per il grafo
    free(g->out);
    for(int i=0; i<g->N; i++){
        free(g->in[i].vettore);
        xpthread_mutex_destroy(&g->mutex[i], index);
    }
    free(g->mutex);
    free(g->in);
    free(g);
}       