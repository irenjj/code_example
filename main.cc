#include <pthread.h>

#include <cassert>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#define N 100000

class RwLock;

std::vector<RwLock> g_locks(N);
std::vector<int64_t> g_nums(N);

class RwLock {
 public:
  RwLock() {
    pthread_rwlock_init(&lock_, nullptr);
  }
  ~RwLock() {
    pthread_rwlock_destroy(&lock_);
  }

  int ReadLock() {
    return pthread_rwlock_tryrdlock(&lock_);
  }
  int WriteLock() {
    return pthread_rwlock_trywrlock(&lock_);
  }
  int Unlock() {
    std::lock_guard<std::mutex> guard(mutex_);

    int err = pthread_rwlock_unlock(&lock_);
    if (!err) {
      set_client_id(0);
    }
    return err;
  }

  int Upgrade(uint64_t client_id) {
    std::lock_guard<std::mutex> guard(mutex_);

    if (client_id != client_id_) {
      return 1;
    }

    assert(!pthread_rwlock_unlock(&lock_));

    int err = pthread_rwlock_trywrlock(&lock_);
    return err;
  }

  bool ValidClientId() const { return client_id_ != 0; }

  uint64_t client_id() const { return client_id_; }
  void set_client_id(uint64_t client_id) {
    client_id_ = client_id;
  }

 private:
  std::mutex mutex_{};
  pthread_rwlock_t lock_{};
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
    if (g_locks[i].ReadLock()) {
      continue;
    } else {
      g_locks[i].set_client_id(client_id);
    }

    if (g_locks[i_1].ReadLock()) {
      g_locks[i].Unlock();
      continue;
    } else {
      g_locks[i_1].set_client_id(client_id);
    }

    if (g_locks[i_2].ReadLock()) {
      g_locks[i].Unlock();
      g_locks[i_1].Unlock();
      continue;
    } else {
      g_locks[i_2].set_client_id(client_id);
    }

    // j may be the same with i/i_1/i_2
    if (g_locks[j].WriteLock()) {
      if (g_locks[j].Upgrade(client_id)) {
        g_locks[i].Unlock();
        g_locks[i_1].Unlock();
        g_locks[i_2].Unlock();
        continue;
      }
    } else {
      g_locks[j].set_client_id(client_id);
    }

    n--;
    long long sum = (g_nums[i] + g_nums[i_1]) % INTMAX_MAX;
    sum = (sum + g_nums[i_2]) % INTMAX_MAX;
    g_nums[j] = sum;

    g_locks[i].Unlock();
    g_locks[i_1].Unlock();
    g_locks[i_2].Unlock();
    g_locks[j].Unlock();
  }
}

int main() {
  InitNums();

  int num = 1000;
  std::vector<std::thread> thread_pool(num);
  for (int i = 0; i < num; i++) {
    thread_pool[i] = std::thread(&ThreadFunc, i+1, N);
  }

  for (int i = 0; i < num; i++) {
    thread_pool[i].join();
  }

  return 0;
}
