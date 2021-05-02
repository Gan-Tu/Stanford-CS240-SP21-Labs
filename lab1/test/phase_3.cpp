#include <chloros.h>
#include <atomic>
#include <common.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <unordered_set>
#include <vector>

std::atomic<int> counter{0};

void Worker(void*) {
  ++counter;
  fprintf(stderr, "\n yield with false  from worker \n");
  ASSERT(chloros::Yield(false) == true);
  fprintf(stderr, "\n resume from yield in worker \n");
  ++counter;
  fprintf(stderr, "\n yield with true from worker \n");
  ASSERT(chloros::Yield(true) == false);
  fprintf(stderr, "\n yield with false last time from worker \n");
  chloros::Yield();
  fprintf(stderr, "\n done \n");
}

void WorkerWithArgument(void* arg) { counter = *reinterpret_cast<int*>(&arg); }

static void CheckYield() {
  fprintf(stderr, "\n check yield 1st \n");
  ASSERT(chloros::Yield() == false);
  fprintf(stderr, "\n spawn worker from yield \n");
  chloros::Spawn(Worker, nullptr);
  ASSERT(counter == 1);
  fprintf(stderr, "\n wait from yield \n");
  chloros::Wait();
  fprintf(stderr, "\n wake up from wait in yield \n");
  ASSERT(counter == 2);
  fprintf(stderr, "\n check yield 2nd \n");
  ASSERT(chloros::Yield() == false);
  fprintf(stderr, "\n CheckYield finished \n");
}

static void CheckSpawn() {
  chloros::Spawn(WorkerWithArgument, reinterpret_cast<void*>(42));
  chloros::Wait();
  ASSERT(counter == 42);
}

void WorkerWithArithmetic(void*) {
  float a = 42;
  counter = sqrt(a);
}

static void CheckStackAlignment() {
  chloros::Spawn(WorkerWithArithmetic, nullptr);
  ASSERT(counter == 6);
}

constexpr int const kLoopTimes = 100;
std::vector<int> loop_values{};

void WorkerYieldLoop(void*) {
  for (int i = 0; i < kLoopTimes; ++i) {
    loop_values.push_back(i);
    chloros::Yield();
  }
}

static void CheckYieldLoop() {
  chloros::Spawn(WorkerYieldLoop, nullptr);
  chloros::Spawn(WorkerYieldLoop, nullptr);
  chloros::Wait();
  ASSERT(loop_values.size() == kLoopTimes * 2);
  for (int i = 0; i < kLoopTimes; ++i) {
    ASSERT(loop_values.at(i * 2) == i);
    ASSERT(loop_values.at(i * 2 + 1) == i);
  }
}

int main() {
  chloros::Initialize();
  CheckYield();
  CheckSpawn();
  CheckStackAlignment();
  CheckYieldLoop();
  LOG("Phase 3 passed!");
  return 0;
}
