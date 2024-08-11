#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct queue_t queue;
typedef struct queue_node_t queue_node;

struct queue_t {
    queue_node *first;
    queue_node *last;
    int size;
    pthread_mutex_t queue_m;
    pthread_cond_t queue_c;
};

struct queue_node_t {
    void *element;
    queue_node *next;
    queue_node *previous;
};

void push(queue *queue, void *element) {
    int pre_push_size;
    queue_node *node = malloc(sizeof(queue_node));

    pthread_mutex_lock(&queue->queue_m);

    pre_push_size = queue->size;

    node->element = element;
    node->previous = NULL;
    node->next = queue->first;

    if(queue->size == 0) {
        queue->last = node;
    } else {
        queue->first->previous = node;
    }

    queue->first = node;
    queue->size++;

    if(pre_push_size == 0) {
        pthread_cond_signal(&queue->queue_c);
    }

    pthread_mutex_unlock(&queue->queue_m);
}

void *pop_unsafe(queue *queue) {
    void *pop_el = queue->last->element;

    queue->last = queue->last->previous;
    
    if(queue->last != NULL) {
        queue->last->next = NULL;
    }

    /*TODO: memory leak*/

    queue->size--;

    return pop_el;
}

void *wait_and_pop(queue *queue) {
    pthread_mutex_lock(&queue->queue_m);

    while(queue->size == 0) {
        pthread_cond_wait(&queue->queue_c, &queue->queue_m);
    }

    void *e = pop_unsafe(queue);

    pthread_mutex_unlock(&queue->queue_m);

    return e;
}

queue *new_queue() {
    queue *q = malloc(sizeof(queue));

    q->first = NULL;
    q->last = NULL;
    q->size = 0;
    q->queue_m = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    q->queue_c = (pthread_cond_t) PTHREAD_COND_INITIALIZER;

    return q;
}

queue *q;

void producer() {
    for(int i = 0; i < 15; i++) {
        int *e = malloc(sizeof(int));
        *e = i;
        push(q, e);
        printf("produce %d\n", i);
        /*sleep(2);*/
    }
}

void consumer() {
    do {
        int *e = wait_and_pop(q);
        printf("consume %d\n", *e);
        sleep(4);
    } while(true);
}

int main() {
    q = new_queue();

    pthread_t producer_t;
    pthread_t consumer_t;

    pthread_create(&producer_t, NULL, &producer, NULL);
    pthread_create(&consumer_t, NULL, &consumer, NULL);

    pthread_join(producer_t, NULL);
    pthread_join(consumer_t, NULL);

    return 0;
}