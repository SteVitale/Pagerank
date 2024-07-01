### Pagerank + PThread ###
Implementazione in C dell'algoritmo "Pagerank", parallelizzato con thread POSIX.

## INPUT ##
./pagerank -d D -t T -e E -k K -m M filename.mtx

*Paramtri opzionali*:  <br />
- D dumping factor <br />
- T number thread aux <br />
- E max error <br />
- M max iteration <br />
- K number of best rank <br />

*Parametri obbligatori*: filename.mtx, formattato nel seguente modo: <br />
- % righe di commento iniziali
- la prima riga non commentata contiene 3 interi (r c n) dove i primi due sono il numero di nodi e il terzo è il numero di archi
- elenco di archi nel formato "%d %d\n"

## OUTPUT ##
Number of nodes: V <br />
Number of dead-end nodes: V <br />
Did not converge after V iterations / Converged after V iterations <br />
Sum of ranks: V    (should be V) <br />
Top V nodes: <br />
  ind1) rank[ind1] <br />
  ind2) rank[ind2] <br />
  ind3) rank[ind3] <br />


## FUNZIONAMENTO ##

1) La lettura del file.mtx avviene tramite lo schema produttore / consumatore, dove l'unico produttore (main) inserisce gli archi
   nel buffer, e i consumatori leggono gli archi e creano il grafo;
2) La parallelizzazione del calcolo del pagerank avviene nel seguente modo:
   1) Il thread principale crea i thread aux necessari per eseguire i calcoli del pagerank ad ogni iterazione T, T+1, ..., fino a raggiungere la convergenza;
   2) Il thread principale avvia i thread aux e segnala l'inizio di una nuova iterazione tramite un semaforo posix (sem_start);
   3) I thread ausiliari, dopo aver ricevuto il segnale, procedono con i calcoli. Ognuno di essi ha accesso ad una variabile condivisa atomica (atomic_int *pos) da cui preleva il valore e incrementa con il batch_size. Dopo aver prelevato il batch il thread aux correte calcola i componenti del pagerank dall'indice start all'indice end e poi riprova ad acquisire un nuovo batch;
   4) Dopo che gli indici sono stati esauriti i thread ausiliari si mettono in coda per incrementare l'errore al passo corrente e il contributo dei nodi dead-end per il T+1. Poi segnalano la fine del lavoro al tempo T con un semaforo posix (sem_end);
   5) Il controllo passa al thread principale che verifica la convergenza e comunica se continuare il calcolo (ripartire dal punto 2.2) o terminare;

*Funzionamento del BATCH*
I thread lavorano su piccoli intervalli, cercando un compromesso tra "lentessa se acquisissero un indice per volta" e "dividere gli indici staticamente vuol dire non parallelizzare".
