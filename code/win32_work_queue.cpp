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
  Queue->SemaphoreHandle = CreateSemaphoreA(0, 20, 20, NULL);
}

void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback,
                     void *Data) {
  uint32 NextWrite = InterlockedExchangeAdd(&Queue->NextWrite, 1) ;
  Assert(((NextWrite + 1) % Queue->Size) != Queue->NextRead % Queue->Size);
  win32_work_queue_task *NewEntry = &Queue->Base[NextWrite% Queue->Size];
  NewEntry->Callback = Callback;
  NewEntry->Data = Data;
  _WriteBarrier();
  _mm_sfence();
  InterlockedIncrement64((LONG64 volatile *)&Queue->CompletionGoal);
  ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal bool DoNextWorkQueueEntry(work_queue *Queue) {
  bool ShouldSleep = false;

  uint32 OriginalNextRead = Queue->NextRead;
  uint32 NewNextRead = (OriginalNextRead + 1);
  if (OriginalNextRead != Queue->NextWrite) {
    uint32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextRead,
                                              NewNextRead, OriginalNextRead);

    if (Index == OriginalNextRead) {
      win32_work_queue_task Task = Queue->Base[Index % Queue->Size];
      work_queue_callback *Callback = Task.Callback;
      Callback(Task.Data);
      InterlockedIncrement64((LONG64 volatile *)&Queue->CompletionCount);
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
