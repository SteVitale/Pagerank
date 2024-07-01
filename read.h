#include "x_func.h"
#include "stdatomic.h"
#define BufSize 1000
#define index __LINE__,__FILE__ 

typedef struct { //singolo elemento del buffer per lettura file.mtx
    int a, b;
}coppia;

typedef struct {
    atomic_int *vettore; //Vettore G->IN[0,1,2...N]
    int dimensione, elementi; //Contatori sul vettore
}inmap;

typedef struct {
    int N; //Numero Nodi
    atomic_int *out; // OUT(j) = |{i:(j,i) in G}|
    inmap *in; // IN(j) = {i:(i,j) in G}
    int invalid_arc; //Numero Archi Duplicati O i==j
    pthread_mutex_t *mutex; //mutex per ogni nodo j
}grafo;

/* STRUTTURA PER SINCRONIZZAZIONE CONSUMATORI*/
typedef struct {
    coppia *buffer;
    int *ind;
    pthread_mutex_t *mutex_buf;
    sem_t *sem_free_slot, *sem_data_item;
    grafo *g;
}thread_cons;

/* BODY CONSUMATORE*/
void *consumatori(void *arg);

/* FUNZIONI PER GESTIONE DEL GRAFO*/
void init_grafo(grafo *g, int dim);
void add_grafo(grafo *g, int i, int j);
void check_size_grafo(grafo *g);
void free_grafo(grafo *g);
