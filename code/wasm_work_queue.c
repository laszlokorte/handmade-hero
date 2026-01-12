#include "wasm_work_queue.h"

inline void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback,
                            void *Data) {
  uint32_t Write =
      atomic_fetch_add_explicit(&Queue->NextWrite, 1, memory_order_relaxed);

  // Ringbuffer voll? => Bug
  uint32_t Read = atomic_load_explicit(&Queue->NextRead, memory_order_acquire);

  // simple safety check
  // (kein Blocking, wie im Original)
  if (((Write + 1) % Queue->Size) == (Read % Queue->Size)) {
    __builtin_trap();
  }

  wasm_work_queue_task *Entry = &Queue->Base[Write % Queue->Size];

  Entry->Callback = Callback;
  Entry->Data = Data;

  atomic_fetch_add_explicit(&Queue->CompletionGoal, 1, memory_order_release);

  // "sem_post"
  atomic_fetch_add_explicit(&Queue->WakeCounter, 1, memory_order_release);

  __builtin_wasm_memory_atomic_notify((int *)&Queue->WakeCounter, 1);
}

/* ===== Worker ===== */

inline int DoNextWorkQueueEntry(work_queue *Queue) {
  uint32_t Read = atomic_load_explicit(&Queue->NextRead, memory_order_relaxed);
  uint32_t Write =
      atomic_load_explicit(&Queue->NextWrite, memory_order_acquire);

  if (Read == Write) {
    return 1; // sleep
  }

  uint32_t Expected = Read;
  if (atomic_compare_exchange_strong_explicit(&Queue->NextRead, &Expected,
                                              Read + 1, memory_order_acq_rel,
                                              memory_order_relaxed)) {
    wasm_work_queue_task Task = Queue->Base[Read % Queue->Size];

    Task.Callback(Task.Data);

    atomic_fetch_add_explicit(&Queue->CompletionCount, 1, memory_order_release);
  }

  return 0;
}

/* ===== Wait ===== */

inline void WaitForQueueToFinish(work_queue *Queue) {
  for (;;) {
    int64_t Done =
        atomic_load_explicit(&Queue->CompletionCount, memory_order_acquire);
    int64_t Goal =
        atomic_load_explicit(&Queue->CompletionGoal, memory_order_acquire);

    if (Done == Goal) {
      return;
    }

    DoNextWorkQueueEntry(Queue);
  }
}

/* ===== Thread proc ===== */

int WorkQueueThreadProc(void *Param) {
  wasm_thread_info *Info = (wasm_thread_info *)Param;
  work_queue *Queue = Info->Queue;

  int workDone = 0;
  for (;;) {
    if (DoNextWorkQueueEntry(Queue)) {
      int32_t v =
          atomic_load_explicit(&Queue->WakeCounter, memory_order_acquire);

      __builtin_wasm_memory_atomic_wait32((int *)&Queue->WakeCounter, v, -1);
    } else {
      workDone++;
    }
  }
  return workDone;
}
