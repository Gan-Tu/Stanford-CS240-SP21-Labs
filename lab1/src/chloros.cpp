#include "chloros.h"
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include "common.h"

extern "C" {

// Assembly code to switch context from `old_context` to `new_context`.
void ContextSwitch(chloros::Context* old_context,
                   chloros::Context* new_context) __asm__("context_switch");

// Assembly entry point for a thread that is spawn. It will fetch arguments on
// the stack and put them in the right registers. Then it will call
// `ThreadEntry` for further setup.
void StartThread(void* arg) __asm__("start_thread");
}

namespace chloros {

namespace {

// Default stack size is 2 MB.
constexpr int const kStackSize{1 << 21};

// Queue of threads that are not running.
std::vector<std::unique_ptr<Thread>> thread_queue{};

// Mutex protecting the queue, which will potentially be accessed by multiple
// kernel threads.
std::mutex queue_lock{};

// Current running thread. It is local to the kernel thread.
thread_local std::unique_ptr<Thread> current_thread{nullptr};

// Thread ID of initial thread. In extra credit phase, we want to switch back to
// the initial thread corresponding to the kernel thread. Otherwise there may be
// errors during thread cleanup. In other words, if the initial thread is
// spawned on this kernel thread, never switch it to another kernel thread!
// Think about how to achieve this in `Yield` and using this thread-local
// `initial_thread_id`.
thread_local uint64_t initial_thread_id;

}  // anonymous namespace

std::atomic<uint64_t> Thread::next_id;

Thread::Thread(bool create_stack)
    : id{next_id++}, state{State::kWaiting}, context{}, stack{nullptr} {
  // FIXME: Phase 1
  if (create_stack) {
    // aligned_alloc gives the beginning of the allocated memory address
    // since the stack grows from higher address to lower address, we will
    // need to re-point the stack pointer to the end of the allocated memory
    // address, aka. the top of the stack
    stack = (uint8_t*) aligned_alloc(16, chloros::kStackSize) + chloros::kStackSize;
  }

  // These two initial values are provided for you.
  context.mxcsr = 0x1F80;
  context.x87 = 0x037F;
}

Thread::~Thread() {
  // FIXME: Phase 1
  if (stack != nullptr) {
    free(stack - chloros::kStackSize);
  }
}

void Thread::PrintDebug() {
  fprintf(stderr, "Thread %" PRId64 ": ", id);
  switch (state) {
    case State::kWaiting:
      fprintf(stderr, "waiting");
      break;
    case State::kReady:
      fprintf(stderr, "ready");
      break;
    case State::kRunning:
      fprintf(stderr, "running");
      break;
    case State::kZombie:
      fprintf(stderr, "zombie");
      break;
    default:
      break;
  }
  fprintf(stderr, "\n\tStack: %p\n", stack);
  fprintf(stderr, "\tRSP: 0x%" PRIx64 "\n", context.rsp);
  fprintf(stderr, "\tR15: 0x%" PRIx64 "\n", context.r15);
  fprintf(stderr, "\tR14: 0x%" PRIx64 "\n", context.r14);
  fprintf(stderr, "\tR13: 0x%" PRIx64 "\n", context.r13);
  fprintf(stderr, "\tR12: 0x%" PRIx64 "\n", context.r13);
  fprintf(stderr, "\tRBX: 0x%" PRIx64 "\n", context.rbx);
  fprintf(stderr, "\tRBP: 0x%" PRIx64 "\n", context.rbp);
  fprintf(stderr, "\tMXCSR: 0x%x\n", context.mxcsr);
  fprintf(stderr, "\tx87: 0x%x\n", context.x87);
}

void Initialize() {
  auto new_thread = std::make_unique<Thread>(false);
  new_thread->state = Thread::State::kWaiting;
  initial_thread_id = new_thread->id;
  current_thread = std::move(new_thread);
}

void Spawn(Function fn, void* arg) {
  auto new_thread = std::make_unique<Thread>(true);

  // FIXME: Phase 3
  // Set up the initial stack, and put it in `thread_queue`. Must yield to it
  // afterwards. How do we make sure it's executed right away?

  size_t offset = sizeof(void**);
  uint64_t current_rsp  = (uint64_t) new_thread->stack;
  // Since current_rsp is at the top of the stack, we need to move stack
  // pointer downwards, and lay the arguments and functions in a top-down
  // manner for the stack layout.
  //
  //      ------------- new_thread->stack (top of stack)
  //          args
  //      ------------- |
  //           fn        |
  //      ------------- |
  //       StartThread  v
  //      ------------- rsp ends here
  //
  //          ....
  //       rest of stack
  //
  //
  // Note that, the assignment writes to the memory upwards, so we need to
  // move the stack pointer down one word, before we write stuff each time.
  // current_rsp += offset;
  current_rsp -= offset;
  *(void**)current_rsp = (void*)arg;

  current_rsp -= offset;
  *(void**)current_rsp = (void*)fn;

  current_rsp -= offset;
  *(void**)current_rsp = (void*)StartThread;

  new_thread->context.rsp = current_rsp;
  new_thread->state = Thread::State::kReady;

  // Push spawned thread to the front, so it can be scheduled next
  queue_lock.lock();
  thread_queue.insert(thread_queue.begin(), std::move(new_thread));
  queue_lock.unlock();

  fprintf(stderr, "\n call Yield from spawn \n");

  Yield();
}

bool Yield(bool only_ready) {
  // FIXME: Phase 3
  // Find a thread to yield to. If `only_ready` is true, only consider threads
  // in `kReady` state. Otherwise, also consider `kWaiting` threads. Be careful,
  // never schedule initial thread onto other kernel threads (for extra credit
  // phase)!

  fprintf(stderr, "\n try getting lock \n");
  queue_lock.lock();

  fprintf(stderr, "\n loop thread queue in yield \n");

  // Find a potential thread to schedule
  std::vector<std::unique_ptr<Thread>>::iterator it = thread_queue.begin();
  while (it != thread_queue.end()) {
    if ((*it)->state == Thread::State::kReady ||
        ((*it)->state == Thread::State::kWaiting && !only_ready)) {
      break;
    }
    it++;
  }

  // Return false, if we cannot yield
  if (it == thread_queue.end()) {
    queue_lock.unlock();
    fprintf(stderr, "\n unlocked \n");
    fprintf(stderr, "\n nothing to yield \n");
    return false;
  }

  std::unique_ptr<Thread> prev_thread = std::move(current_thread);
  std::unique_ptr<Thread> next_thread = std::move(*it);
  thread_queue.erase(it);

  // Update thread states
  if (prev_thread->state == Thread::State::kRunning) {
    prev_thread->state = Thread::State::kReady;
  }
  next_thread->state = Thread::State::kRunning;

  // Keep context reference for later
  Context* prev_context = &prev_thread->context;

  // Update thread queue
  thread_queue.push_back(std::move(prev_thread));
  current_thread = std::move(next_thread);

  // Context Switch
  queue_lock.unlock();
  ContextSwitch(prev_context, &(current_thread->context));
  GarbageCollect();

  return true;
}

void Wait() {
  current_thread->state = Thread::State::kWaiting;
  while (Yield(true)) {
    current_thread->state = Thread::State::kWaiting;
  }
}

void GarbageCollect() {
  // FIXME: Phase 4
  std::lock_guard<std::mutex> lock{queue_lock};

  // We do not erase in place because it is a bad idea
  // to modify the vector using iterator, while u iterate
  std::vector<std::unique_ptr<Thread>> new_thread_queue{};
  for (auto&& thread : thread_queue) {
    if (thread->state != Thread::State::kZombie) {
      new_thread_queue.push_back(std::move(thread));
    }
  }

  thread_queue.clear();
  thread_queue = std::move(new_thread_queue);
}

std::pair<int, int> GetThreadCount() {
  // Please don't modify this function.
  int ready = 0;
  int zombie = 0;
  std::lock_guard<std::mutex> lock{queue_lock};
  for (auto&& i : thread_queue) {
    if (i->state == Thread::State::kZombie) {
      ++zombie;
    } else {
      ++ready;
    }
  }
  return {ready, zombie};
}

void ThreadEntry(Function fn, void* arg) {
  fn(arg);
  current_thread->state = Thread::State::kZombie;
  // LOG_DEBUG("Thread %" PRId64 " exiting.", current_thread->id);
  // A thread that is spawn will always die yielding control to other threads.
  chloros::Yield();
  // Unreachable here. Why?
  ASSERT(false);
}

}  // namespace chloros
