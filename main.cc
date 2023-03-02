#include <cassert>
#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#define N 100000

class RwLock;

std::vector<RwLock> g_locks(N);
std::vector<int64_t> g_nums(N);
int g_num = 100;

class RwLock {
 public:
  RwLock() = default;
  ~RwLock() = default;

  bool ReadLock() {
    std::lock_guard<std::mutex> guard(mutex_);

    bool locked = lock_.try_lock_shared();
    if (!locked) {
      return false;
    }

    client_num_++;
    assert(client_num_ <= g_num);
    return true;
  }
  void ReadUnlock() {
    std::lock_guard<std::mutex> guard(mutex_);

    client_num_--;
    assert(client_num_ >= 0);
    lock_.unlock_shared();
  }

  bool WriteLock() {
    std::lock_guard<std::mutex> guard(mutex_);

    bool locked = lock_.try_lock();
    if (!locked) {
      return false;
    }

    client_num_++;
    assert(client_num_ <= g_num);
    return true;
  }
  void WriteUnlock() {
    std::lock_guard<std::mutex> guard(mutex_);

    client_num_--;
    assert(client_num_ >= 0);
    lock_.unlock();
  }

  bool Upgrade() {
    std::lock_guard<std::mutex> guard(mutex_);

    if (client_num_ > 1) {
      return false;
    }

    lock_.unlock_shared();
    lock_.lock();
    return true;
  }

 private:
  std::mutex mutex_{};

  std::shared_mutex lock_{};
  uint64_t client_num_ = 0;
};

void InitNums() {
  for (int i = 0; i < N; i++) {
    g_nums[i] = rand() % 1000;
  }
}

void ThreadFunc(int client_id, int n) {
  while (n) {
    int i = rand() % 100000;
    int i_1 = i + 1 >= 100000 ? (i + 1) % 100000 : (i + 1);
    int i_2 = i + 2 >= 100000 ? (i + 2) % 100000 : (i + 2);

    int j = rand() % 100000;

    // i, i_1, i_2 can't be same
    if (!g_locks[i].ReadLock()) {
      continue;
    }

    if (!g_locks[i_1].ReadLock()) {
      g_locks[i].ReadUnlock();
      continue;
    }

    if (!g_locks[i_2].ReadLock()) {
      g_locks[i].ReadUnlock();
      g_locks[i_1].ReadUnlock();
      continue;
    }

    // j may be the same with i/i_1/i_2
    if (!g_locks[j].WriteLock()) {
      if ((j != i && j != i_1 && j != i_2) || !g_locks[j].Upgrade()) {
        g_locks[i].ReadUnlock();
        g_locks[i_1].ReadUnlock();
        g_locks[i_2].ReadUnlock();
        continue;
      }
    }

    n--;
    long long sum = (g_nums[i] + g_nums[i_1]) % INTMAX_MAX;
    sum = (sum + g_nums[i_2]) % INTMAX_MAX;
    g_nums[j] = sum;

    g_locks[i].ReadUnlock();
    g_locks[i_1].ReadUnlock();
    g_locks[i_2].ReadUnlock();
    g_locks[j].WriteUnlock();
  }
}

int main() {
  InitNums();

  std::vector<std::thread> thread_pool(g_num);
  for (int i = 0; i < g_num; i++) {
    thread_pool[i] = std::thread(&ThreadFunc, i + 1, N);
  }

  for (int i = 0; i < g_num; i++) {
    thread_pool[i].join();
  }

  return 0;
}
