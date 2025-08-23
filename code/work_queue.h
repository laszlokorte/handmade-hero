#ifndef WORK_QUEUE_H

typedef void work_queue_callback(void *Data);

struct work_queue;

void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback, void *Data);

void WaitForQueueToFinish(work_queue *Queue, bool Participate);

#define WORK_QUEUE_H
#endif
