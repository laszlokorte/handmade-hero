#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>

#include "macos_handmade.h"

inline void InitializeWorkQueue(work_queue *Queue, size_t Size,
                                macos_work_queue_task *Base) {
  Queue->Size = Size;
  Queue->Base = Base;
  atomic_store(&Queue->NextWrite, 0);
  atomic_store(&Queue->NextRead, 0);
  atomic_store(&Queue->CompletionGoal, 0);
  atomic_store(&Queue->CompletionCount, 0);
  Queue->Semaphore = dispatch_semaphore_create(0);
}

inline void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback,
                            void *Data) {
  unsigned int NextWrite = atomic_fetch_add(&Queue->NextWrite, 1);
  assert(((NextWrite + 1) % Queue->Size) !=
         atomic_load(&Queue->NextRead) % Queue->Size);

  macos_work_queue_task *NewEntry = &Queue->Base[NextWrite % Queue->Size];
  NewEntry->Callback = Callback;
  NewEntry->Data = Data;

  atomic_thread_fence(memory_order_seq_cst);

  atomic_fetch_add(&Queue->CompletionGoal, 1);
  dispatch_semaphore_signal(Queue->Semaphore);
}

inline int DoNextWorkQueueEntry(work_queue *Queue) {
  int ShouldSleep = 0;
  unsigned int OriginalNextRead = atomic_load(&Queue->NextRead);
  unsigned int NextWrite = atomic_load(&Queue->NextWrite);

  if (OriginalNextRead != NextWrite) {
    unsigned int Expected = OriginalNextRead;
    if (atomic_compare_exchange_strong(&Queue->NextRead, &Expected,
                                       Expected + 1)) {
      macos_work_queue_task Task = Queue->Base[Expected % Queue->Size];
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
  macos_thread_info *ThreadInfo = (macos_thread_info *)Param;
  work_queue *Queue = ThreadInfo->Queue;

  for (;;) {
    if (DoNextWorkQueueEntry(Queue)) {
      dispatch_semaphore_wait(Queue->Semaphore, DISPATCH_TIME_FOREVER);
    }
  }
  return NULL;
}
