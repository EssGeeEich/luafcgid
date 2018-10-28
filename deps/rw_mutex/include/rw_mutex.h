#ifndef RW_MUTEX_H
#define RW_MUTEX_H
#include "spinlock_mutex.h"
#include <thread>
#include <string>
#include <set>

enum RWMutex_Mode {
    MX_RELEASED = 0,
    
    MX_READ_ONLY,
    MX_READ_ONLY_AWAITING_WRITE,
    
    MX_READ_WRITE,
    MX_READ_WRITE_AWAITING_WRITE,
    
    MX_HANDOFF_WRITE
};

class rw_mutex {
    spinlock_mutex m_mutex;
    RWMutex_Mode m_mode;
	std::set<std::thread::id> m_users;
	std::thread::id m_handoff_write_thread;
	
	void i_add_self();
	void i_rem_self();
	bool i_has_self() const;
	
	void i_unlock();
	void i_lock();
	void i_lock_read();
public:
    rw_mutex();
    
    void lock_read();
    bool trylock_read();
    
    void lock();
    bool trylock();
    
	// Change from read-lock to write-lock
	void chlock_w();
	
	// Change from write-lock to read-lock
	void chlock_r();
	
    void unlock();
};

#endif // RW_MUTEX_H
