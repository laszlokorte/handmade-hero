#ifndef WORK_QUEUE_H

typedef void work_queue_callback(void *Data);

typedef struct work_queue work_queue;

typedef work_queue work_queue;
struct work_queue;

#define PUSH_TASK_TO_QUEUE(Name)                                               \
  void(Name)(struct work_queue * Queue, work_queue_callback * Callback,        \
             void *Data)

typedef PUSH_TASK_TO_QUEUE(platform_push_task_to_queue);
PUSH_TASK_TO_QUEUE(PushTaskToQueue);

#define WAIT_FOR_QUEUE_TO_FINISH(Name) void Name(struct work_queue *Queue)
WAIT_FOR_QUEUE_TO_FINISH(WaitForQueueToFinish);
typedef WAIT_FOR_QUEUE_TO_FINISH(platform_wait_for_queue_to_finish);



#define WORK_QUEUE_H
#endif
