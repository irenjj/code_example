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

class RwLock {
 public:
  RwLock() = default;
  ~RwLock() = default;

  bool ReadLock() {
    return lock_.try_lock_shared();
  }
  bool WriteLock() {
    return lock_.try_lock();
  }
  void ReadUnlock() {
    std::lock_guard<std::mutex> guard(mutex_);

    set_client_id(0);
    lock_.unlock_shared();
  }
  void WriteUnlock() {
    std::lock_guard<std::mutex> guard(mutex_);

    set_client_id(0);
    lock_.unlock();
  }

  bool Upgrade(uint64_t client_id) {
    std::lock_guard<std::mutex> guard(mutex_);

    if (client_id != client_id_) {
      return false;
    }

    lock_.unlock_shared();
    lock_.lock();
    return true;
  }

  void set_client_id(uint64_t client_id) {
    client_id_ = client_id;
  }

 private:
  std::mutex mutex_{};

  std::shared_mutex lock_{};
  uint64_t client_id_ = 0;
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
    } else {
      g_locks[i].set_client_id(client_id);
    }

    if (!g_locks[i_1].ReadLock()) {
      g_locks[i].ReadUnlock();
      continue;
    } else {
      g_locks[i_1].set_client_id(client_id);
    }

    if (!g_locks[i_2].ReadLock()) {
      g_locks[i].ReadUnlock();
      g_locks[i_1].ReadUnlock();
      continue;
    } else {
      g_locks[i_2].set_client_id(client_id);
    }

    // j may be the same with i/i_1/i_2
    if (!g_locks[j].WriteLock()) {
      if (!g_locks[j].Upgrade(client_id)) {
        g_locks[i].ReadUnlock();
        g_locks[i_1].ReadUnlock();
        g_locks[i_2].ReadUnlock();
        continue;
      }
    } else {
      g_locks[j].set_client_id(client_id);
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

  int num = 10;
  std::vector<std::thread> thread_pool(num);
  for (int i = 0; i < num; i++) {
    thread_pool[i] = std::thread(&ThreadFunc, i+1, N);
  }

  for (int i = 0; i < num; i++) {
    thread_pool[i].join();
  }

  return 0;
}
