#include "win32_handmade.h"
#include <intrin.h>

internal void InitializeWorkQueue(work_queue *Queue, size_t Size,
                                  win32_work_queue_task *Base) {
  Queue->Size = Size;
  Queue->Base = Base;
  Queue->NextWrite = 0;
  Queue->NextRead = 0;
  Queue->CompletionGoal = 0;
  Queue->CompletionCount = 0;
  //  TODO create semaphore correctly
  Queue->SemaphoreHandle = CreateSemaphoreA(0, 8, 8, NULL);
}

void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback,
                     void *Data) {
  uint32 NextWrite = Queue->NextWrite;
  uint32 NewNextWrite = (Queue->NextWrite + 1) % Queue->Size;
  Assert(NewNextWrite != Queue->NextRead);
  win32_work_queue_task *NewEntry = &Queue->Base[NextWrite];
  NewEntry->Callback = Callback;
  NewEntry->Data = Data;
  ++Queue->CompletionGoal;
  _WriteBarrier();
  _mm_sfence();
  Queue->NextWrite = NewNextWrite;
  ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal bool DoNextWorkQueueEntry(work_queue *Queue) {
  bool ShouldSleep = false;

  uint32 OriginalNextRead = Queue->NextRead;
  uint32 NewNextRead = (OriginalNextRead + 1) % Queue->Size;
  if (OriginalNextRead != Queue->NextWrite) {
    uint32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextRead,
                                              NewNextRead, OriginalNextRead);

    if (Index == OriginalNextRead) {
      win32_work_queue_task Task = Queue->Base[Index];
      work_queue_callback *Callback = Task.Callback;
      Callback(Task.Data);
      InterlockedIncrement((LONG volatile *)&Queue->CompletionCount);
    }
  } else {
    ShouldSleep = true;
  }

  return ShouldSleep;
}

void WaitForQueueToFinish(work_queue *Queue) {
  while (Queue->CompletionCount != Queue->CompletionGoal) {
    DoNextWorkQueueEntry(Queue);
  }

  Queue->CompletionCount = 0;
  Queue->CompletionGoal = 0;
}

DWORD WINAPI WorkQueueThreadProc(LPVOID lpParameter) {
  win32_thread_info *ThreadInfo = (win32_thread_info *)lpParameter;

  for (;;) {
    if (DoNextWorkQueueEntry(ThreadInfo->Queue)) {
      WaitForSingleObjectEx(ThreadInfo->Queue->SemaphoreHandle, INFINITE,
                            false);
    }
  }
}
