#include <stdatomic.h>
#include <stdint.h>

#include "work_queue.h"

typedef void work_queue_callback(void *Data);

/* ===== Task ===== */

typedef struct wasm_work_queue_task {
  work_queue_callback *Callback;
  void *Data;
} wasm_work_queue_task;

/* ===== Queue ===== */

typedef struct work_queue {
  uint32_t Size;
  wasm_work_queue_task *Base;

  _Atomic uint32_t NextWrite;
  _Atomic uint32_t NextRead;
  _Atomic int64_t CompletionGoal;
  _Atomic int64_t CompletionCount;

  // Ersatz für sem_t
  _Atomic int32_t WakeCounter;
} work_queue;

/* ===== Thread info ===== */

typedef struct wasm_thread_info {
  int32_t LogicalThreadIndex;
  work_queue *Queue;
} wasm_thread_info;

/* ===== Push ===== */

inline void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback,
                            void *Data);

/* ===== Worker ===== */

inline int DoNextWorkQueueEntry(work_queue *Queue);

/* ===== Wait ===== */

inline void WaitForQueueToFinish(work_queue *Queue);

/* ===== Thread proc ===== */

int WorkQueueThreadProc(void *Param);
