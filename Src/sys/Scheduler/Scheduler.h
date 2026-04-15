#ifndef PROJECT_ETHOS_SCHEDULER_H
#define PROJECT_ETHOS_SCHEDULER_H
#include <stdbool.h>

#define MAX_SIZE 40 

typedef struct Queue {
    int process_ids[MAX_SIZE];
    int front;
    int rear;
    int count;
} Queue;

void initQueue(Queue* q);
bool isEmpty(Queue* q);
void enqueue(Queue* q, int process_id);
int dequeue(Queue* q);
void removeFromGeneralQueue(Queue* q, int process_id);

#endif