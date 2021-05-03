#include <atomic>
#include <chloros.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

constexpr int const kKernelThreads = 5;

struct ExampleData {
  bool is_ready;
  std::string value;
};

std::mutex data_lock{};
std::unique_ptr<ExampleData> example_data{nullptr};

void ReaderWriterThread(void *arg) {
  int n = *reinterpret_cast<int *>(&arg);
  printf("Reader Writer thread %d starts running.\n", n);
  if (!example_data->is_ready) {
    data_lock.lock();
    if (!example_data->is_ready) {
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      char buff[100];
      sprintf(buff, "Hello from thread %d", n);
      example_data->value = std::string(buff);
      example_data->is_ready = true;
      printf("example_data is initialized by thread %d\n", n);
    }
    data_lock.unlock();
  }
  printf("The value read is %s by thread %d\n", example_data->value.c_str(), n);
}

void ThreadWorker(int n) {
  chloros::Initialize();
  chloros::Spawn(ReaderWriterThread, reinterpret_cast<void *>(n));
  chloros::Wait();
  printf("Finished thread worker\n");
}

int main() {
  example_data = std::make_unique<ExampleData>();
  example_data->is_ready = false;
  example_data->value = "<uninitialize_data>";

  std::vector<std::thread> threads{};
  for (int i = 0; i < kKernelThreads; ++i) {
    threads.emplace_back(ThreadWorker, i);
  }

  for (int i = 0; i < kKernelThreads; ++i) {
    threads[i].join();
  }
  return 0;
}
