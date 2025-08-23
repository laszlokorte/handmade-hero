#include "work_queue.h"

struct win32_work_queue_task {
    bool isValid;
    work_queue_callback *Callback;
    void *Data;
};

struct work_queue {
    size_t Size;
    size_t Base;
    size_t Head;
    size_t Tail;
};

void InitializeWorkQueue(work_queue *Queue, size_t Base, size_t Size) {
    Queue->Size = Size;
    Queue->Base = Base;
    Queue->Head = Base;
    Queue->Tail= Base;
}

void PushTaskToQueue(work_queue *Queue, work_queue_callback *Callback, void *Data) {

}

void WaitForQueueToFinish(work_queue *Queue, bool Participate) {

}
