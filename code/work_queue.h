#ifndef WORK_QUEUE_H

typedef void work_queue_callback(void *Data);

struct work_queue;

void PushTaskToQueue(struct work_queue *Queue, work_queue_callback *Callback,
                     void *Data);

void WaitForQueueToFinish(struct work_queue *Queue);

#define WORK_QUEUE_H
#endif
