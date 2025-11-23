#if !defined(LINUX_WORK_QUEUE_H)
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <stdlib.h>
#include <atomic>
#include "handmade.h"

struct linux_work_queue_task {
  work_queue_callback *Callback;
  void *Data;
};

struct work_queue {
  size_t Size;
  linux_work_queue_task *Base;

  std::atomic<unsigned int> NextWrite;
  std::atomic<unsigned int> NextRead;
  std::atomic<long long> CompletionGoal;
  std::atomic<long long> CompletionCount;

  sem_t Semaphore;
};

struct linux_thread_info {
  int32 LogicalThreadIndex;
  uint32 ThreadId;
  work_queue *Queue;
};

struct linux_thread_pool {
  size_t Count;
  linux_thread_info *Threads;
};
#define LINUX_WORK_QUEUE_H
#endif
