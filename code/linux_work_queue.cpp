#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <stdlib.h>

#include "wayland_handmade.h"

inline void InitializeWorkQueue(work_queue *Queue, size_t Size,
                                linux_work_queue_task *Base) {
  Queue->Size = Size;
  Queue->Base = Base;
  Queue->NextWrite.store(0, std::memory_order_seq_cst);
  Queue->NextRead.store(0, std::memory_order_seq_cst);
  Queue->CompletionGoal.store(0, std::memory_order_seq_cst);
  Queue->CompletionCount.store(0, std::memory_order_seq_cst);
  sem_init(&Queue->Semaphore, 0, 0);
}

inline void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback,
                            void *Data) {
  unsigned int NextWrite = atomic_fetch_add(&Queue->NextWrite, 1);
  assert(((NextWrite + 1) % Queue->Size) !=
         atomic_load(&Queue->NextRead) % Queue->Size);

  linux_work_queue_task *NewEntry = &Queue->Base[NextWrite % Queue->Size];
  NewEntry->Callback = Callback;
  NewEntry->Data = Data;

  atomic_thread_fence(std::memory_order_seq_cst);

  atomic_fetch_add(&Queue->CompletionGoal, 1);
  sem_post(&Queue->Semaphore);
}

inline int DoNextWorkQueueEntry(work_queue *Queue) {
  int ShouldSleep = 0;
  unsigned int OriginalNextRead = atomic_load(&Queue->NextRead);
  unsigned int NextWrite = atomic_load(&Queue->NextWrite);

  if (OriginalNextRead != NextWrite) {
    unsigned int Expected = OriginalNextRead;
    if (atomic_compare_exchange_strong(&Queue->NextRead, &Expected,
                                       Expected + 1)) {
      linux_work_queue_task Task = Queue->Base[Expected % Queue->Size];
      Task.Callback(Task.Data);
      atomic_fetch_add(&Queue->CompletionCount, 1);
    }
  } else {
    ShouldSleep = 1;
  }
  return ShouldSleep;
}

inline void WaitForQueueToFinish(work_queue *Queue) {
  while (atomic_load(&Queue->CompletionCount) !=
         atomic_load(&Queue->CompletionGoal)) {
    DoNextWorkQueueEntry(Queue);
  }
}

void *WorkQueueThreadProc(void *Param) {
  linux_thread_info *ThreadInfo = (linux_thread_info *)Param;
  work_queue *Queue = ThreadInfo->Queue;

  for (;;) {
    if (DoNextWorkQueueEntry(Queue)) {
      sem_wait(&Queue->Semaphore);
    }
  }
  return NULL;
}
