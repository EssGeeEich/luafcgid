#ifndef SPINLOCK_MUTEX_H
#define SPINLOCK_MUTEX_H
#include <atomic>
#include <mutex>

class spinlock_mutex {
    std::atomic_flag m_flag;
public:
    inline spinlock_mutex() noexcept :
        m_flag(ATOMIC_FLAG_INIT) {}
    inline void lock() noexcept {
        while(m_flag.test_and_set(std::memory_order_acquire))
            ;
    }
    inline bool trylock() noexcept {
        return m_flag.test_and_set(std::memory_order_acquire) == 0;
    }
    inline void unlock() noexcept {
        m_flag.clear(std::memory_order_release);
    }
};
typedef std::lock_guard<spinlock_mutex> spinlock_guard;

#endif // SPINLOCK_MUTEX_H
