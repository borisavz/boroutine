#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

typedef enum instr_code_t instr_code;
typedef struct instr_t instr;
typedef struct brt_t brt;

enum instr_code_t {
    CPU,
    IO
};

struct instr_t {
    instr_code code;
    int param;
};

struct brt_t {
    instr *instr_a;
    int len;
    int curr;
};

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
        pthread_cond_broadcast(&queue->queue_c);
    }

    pthread_mutex_unlock(&queue->queue_m);
}

void push_unsafe(queue *queue, void *element) {
    queue_node *node = malloc(sizeof(queue_node));

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

void *pop(queue *queue) {
    pthread_mutex_lock(&queue->queue_m);

    void *pop_el = pop_unsafe(queue);

    pthread_mutex_unlock(&queue->queue_m);

    return pop_el;
}

void *try_pop(queue *queue) {
    pthread_mutex_lock(&queue->queue_m);

    void *pop_el;

    if(queue->size == 0) {
        pop_el = NULL;
    } else {
        pop_el = pop_unsafe(queue);
    }

    pthread_mutex_unlock(&queue->queue_m);

    return pop_el;
}

void *wait_and_pop(queue *queue) {
    while(true) {
        pthread_mutex_lock(&queue->queue_m);

        while(queue->size == 0) {
            pthread_cond_wait(&queue->queue_c, &queue->queue_m);
        }

        pthread_mutex_unlock(&queue->queue_m);

        void *e = try_pop(queue);

        if(e != NULL) {
            return e;
        }
    }
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

queue *global_q;
queue *io_q;

int NUM_OF_PROCS = 15;

void run_brt(brt *b) {
    while(b->curr < b->len) {
        switch (b->instr_a[b->curr].code){
            case CPU:
                printf("cpu %d secs start\n", b->instr_a[b->curr].param);
                sleep(b->instr_a[b->curr].param);
                printf("cpu %d secs end\n", b->instr_a[b->curr].param);
                b->curr++;
                
                break;
            case IO:
                printf("wait io\n");
                push(io_q, b);

                return;
        }
    }

    printf("brt end\n");
}

void run_proc() {
    printf("run proc\n");

    queue *local_q = new_queue();

    while(true) {
        if(local_q->size == 0) {
            void *new_e = wait_and_pop(global_q);
            push_unsafe(local_q, new_e);
        }

        brt *b = pop_unsafe(local_q);

        run_brt(b);
    }
}

void run_io_poller() {
    printf("run io poll\n");

    while(true) {
        sleep(10);

        pthread_mutex_lock(&io_q->queue_m);

        while (io_q->size > 0) {
            brt *b = pop_unsafe(io_q);
            b->curr++;

            push(global_q, b);
            printf("io complete\n");
        }

        pthread_mutex_unlock(&io_q->queue_m);
    }
}

void publish_brt(brt *b) {
    printf("publish brt\n");
    push(global_q, b);
}

int main() {
    global_q = new_queue();
    io_q = new_queue();

    pthread_t *procs = malloc(NUM_OF_PROCS * sizeof(pthread_t));
    pthread_t io_t;

    pthread_create(&io_t, NULL, &run_io_poller, NULL);

    for(int i = 0; i < NUM_OF_PROCS; i++) {
        pthread_create(&procs[i], NULL, &run_proc, NULL);
    }

    instr instructions[] = {
        {CPU, 10},
        {IO, 0},
        {CPU, 30}
    };

    for(int i = 0; i < 50; i++) {
        brt *b1 = malloc(sizeof(brt));
        
        b1->instr_a = instructions;
        b1->len = 3;
        b1->curr = 0;

        publish_brt(b1);
    }

    pthread_join(io_t, NULL);

    for(int i = 0; i < NUM_OF_PROCS; i++) {
        pthread_join(procs[i], NULL);
    }

    return 0;
}