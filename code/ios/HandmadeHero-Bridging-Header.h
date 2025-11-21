//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>

#include "handmade.h"
#include "work_queue.h"

typedef struct ios_work_queue_task {
  work_queue_callback *Callback;
  void *Data;
} ios_work_queue_task;

typedef struct work_queue {
  size_t Size;
  ios_work_queue_task *Base;

  unsigned int NextWrite;
  unsigned int NextRead;
  long long CompletionGoal;
  long long CompletionCount;

  // dispatch_semaphore_t Semaphore;
} work_queue;

typedef struct ios_thread_info {
  int32 LogicalThreadIndex;
  uint32 ThreadId;
  work_queue *Queue;
} ios_thread_info;

typedef struct ios_thread_pool {
  size_t Count;
  ios_thread_info *Threads;
} ios_thread_pool;

PUSH_TASK_TO_QUEUE(iOSPlatformPushTaskToQueue) {}

WAIT_FOR_QUEUE_TO_FINISH(iOSPlatformWaitForQueueToFinish) {}



DEBUG_PLATFORM_FREE_FILE_MEMORY(iOSDebugPlatformFreeFileMemory) {

}

DEBUG_PLATFORM_READ_ENTIRE_FILE(iOSDebugPlatformReadEntireFile) {
  debug_read_file_result Result = {0};

  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(iOSDebugPlatformWriteEntireFile) {
  bool Result = false;

  return Result;
}

#pragma pack(push, 1)
typedef struct {
  float pos[2];
  float col[4];
  float tex[3];
} MetalVertex;

typedef struct {
  float scaleX;
  float scaleY;
  float transX;
  float transY;
} MetalUniforms;
#pragma pack(pop)
