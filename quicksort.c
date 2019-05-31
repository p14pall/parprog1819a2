#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


#define LEN 100000		// size of message queue
#define N 4				// size of thread pool
#define SIZE 100		// size of array to be sorted
#define THRESHOLD 10	// threshold for inssort

//message types
#define SORT 0
#define DONE 1
#define SHUTDOWN 2

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t msg_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t msg_out = PTHREAD_COND_INITIALIZER;

typedef struct msg{
	int type;
	int start;
	int end;
};

struct msg mqueue[LEN];
int in = 0, out = 0;
int m_count = 0;

//function to send messages
void send(int type, int start, int end){
	pthread_mutex_lock(&mutex);
	while (m_count >= LEN){
		pthread_cond_wait(&msg_out, &mutex);
    }
    
	//enqueue
	mqueue[in].type = type;
	mqueue[in].start = start;
	mqueue[in].end = end;
	in = (in + 1) % LEN;
	m_count++;

	pthread_cond_signal(&msg_in);
	pthread_mutex_unlock(&mutex);
}

//function to receive messages
void recv(int *type, int *start, int *end){
	pthread_mutex_lock(&mutex);
	while (m_count < 1){
		pthread_cond_wait(&msg_in, &mutex);
    }

	//dequeue
	*type = mqueue[out].type;
	*start = mqueue[out].start;
	*end = mqueue[out].end;
	out = (out + 1) % LEN;
	m_count--;

	pthread_cond_signal(&msg_out);
	pthread_mutex_unlock(&mutex);
}

//partition function
int partition(double *a, int n){
	int first = 0;
	int middle = n/2;
	int last = n-1;
	double t;

	if (a[middle]<a[first]) { t = a[middle]; a[middle] = a[first]; a[first] = t; }
	if (a[last]<a[middle]) { t = a[last]; a[last] = a[middle]; a[middle] = t; }
	if (a[middle]<a[first]) { t = a[middle]; a[middle] = a[first]; a[first] = t; }
	
	double p = a[middle];
	int i, j;
	for (i=1, j=n-2;; i++, j--){
		while (a[i] < p) i++;
		while (a[j] > p) j--;
		if (i>=j) break;
		t = a[i]; a[i] = a[j]; a[j] = t;
    }
	return i;
}

//insertion sort function
void inssort(double *a, int n){
	int i, j;
	double t;
	for (i=1; i<n; i++){
		j = i;
		while (j>0 && a[j-1] > a[j]){
			t = a[j-1];
			a[j-1] = a[j];
			a[j] = t;
            j--;    
		}
	}
}

//quicksort function
void *quicksort(void *params){
	double *a = (double*) params;
    
	//check for messages
	int t, s, e;
	recv(&t, &s, &e);
	while (t != SHUTDOWN){
	if (t == DONE) {
			//forward to main
			send(DONE, s, e);
		}else if (t == SORT){
			if (e-s <= THRESHOLD){
				//use inssort if <= THRESHOLD
				inssort(a+s, e-s);
				//send DONE for range sorted
				send(DONE, s, e);
			}else{
				//partition if >= THRESHOLD
				int p = partition(a+s, e-s);
				send(SORT, s, s+p);
				send(SORT, s+p, e);
			}
		}
		//check for messages
		recv(&t, &s, &e);
	}
	//when SHUTDOWN received
	send(SHUTDOWN, 0, 0);
	printf("done!\n");
	pthread_exit(NULL);
}

int main(){

	//allocate memory for array
	double *a = (double*) malloc(sizeof(double) * SIZE);
	if (a == NULL){
		printf("Error while allocating memory\n");
		exit(1);
	}

	//initialize array
	for (int i=0; i<SIZE; i++){
		a[i] = (double) rand()/RAND_MAX;
	}

	//create threadpool
	pthread_t threads[LEN];
	for (int i=0; i<LEN; i++){
		if (pthread_create(&threads[i], NULL, quicksort, a) != 0){
			printf("Thread creation error\n");
			free(a);
			exit(1);
		}
	}

	//first workload
	send(SORT, 0, SIZE);

	int t, s, e;
	int count = 0;
	recv(&t, &s, &e);
	while (1){
		if (t == DONE){
			//count sorted elements
			count += e-s;
			printf("Sorted %d out of %d\n", count, SIZE);
			printf("Sorted from %d to %d\n", s, e);
			if (count == SIZE){
				//if everything has been sorted stop
				break;
			}
		}else{
			//continue work
			send(t, s, e);
		}
		//check for messages
		recv(&t, &s, &e);
	}
	//everything sorted
	send(SHUTDOWN, 0, 0);

	//wait for threads to shutdown
	for (int i=0; i<N; i++){
		pthread_join(threads[i], NULL);
    }

	//check sorting
	int i;
	for (i=0; i<SIZE-1; i++){
		if (a[i] > a[i+1]){
			printf("Sort failed!\n");
			break;
		}
	}
	if (i == SIZE-1){
		printf("Sucess!\n");
	}

	//free everything
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&msg_in);
	pthread_cond_destroy(&msg_out);
	free(a);
	
	return 0;
}