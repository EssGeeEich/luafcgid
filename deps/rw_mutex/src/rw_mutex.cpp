#include "rw_mutex.h"
#include <iostream>
#include <thread>
#include <stdexcept>

inline bool can_start_reading(RWMutex_Mode mode)
{
	return mode == MX_READ_ONLY ||
			mode == MX_RELEASED;
}

inline bool can_start_writing(RWMutex_Mode mode, std::thread::id const& hot, std::thread::id const& me)
{
	return mode == MX_RELEASED ||
			((mode == MX_HANDOFF_WRITE) && (hot == me));
}

inline bool can_start_writing(RWMutex_Mode mode)
{
	return mode == MX_RELEASED;
}

inline void do_yield()
{
	std::this_thread::yield();
}

void rw_mutex::i_add_self()
{
	if(m_users.insert(std::this_thread::get_id()).second == false)
		throw std::runtime_error("A thread has locked a rw_mutex twice!");
}

void rw_mutex::i_rem_self()
{
	auto f_iter = m_users.find(std::this_thread::get_id());
	if(f_iter == m_users.end())
		throw std::runtime_error("A thread has unlocked a rw_mutex twice!");
	m_users.erase(f_iter);
}

bool rw_mutex::i_has_self() const
{
	return m_users.find(std::this_thread::get_id()) != m_users.end();
}

void rw_mutex::i_unlock()
{
	i_rem_self();
	
	switch(m_mode)
	{
	case MX_READ_ONLY:
	case MX_READ_WRITE:
		if(m_users.empty())
			m_mode = MX_RELEASED;
		break;
	case MX_READ_WRITE_AWAITING_WRITE:
	case MX_READ_ONLY_AWAITING_WRITE:
		if(m_users.empty())
			m_mode = MX_HANDOFF_WRITE;
		break;
	case MX_RELEASED:
	case MX_HANDOFF_WRITE:
	default:
		throw std::runtime_error("Invalid rw_mutex state!");
	}
}

void rw_mutex::i_lock()
{
	std::thread::id tid = std::this_thread::get_id();
	while(!can_start_writing(m_mode, m_handoff_write_thread, tid))
	{
		if(m_mode == MX_READ_ONLY)
		{
			m_mode = MX_READ_ONLY_AWAITING_WRITE;
			m_handoff_write_thread = tid;
		}
		else if(m_mode == MX_READ_WRITE)
		{
			m_mode = MX_READ_WRITE_AWAITING_WRITE;
			m_handoff_write_thread = tid;
		}
		
		m_mutex.unlock();
		do_yield();
		m_mutex.lock();
	}
	
	i_add_self();
	m_mode = MX_READ_WRITE;
}

void rw_mutex::i_lock_read()
{
	while(!can_start_reading(m_mode))
	{
		m_mutex.unlock();
		do_yield();
		m_mutex.lock();
	}
	
	i_add_self();
	m_mode = MX_READ_ONLY;
}


rw_mutex::rw_mutex() : m_mode(MX_RELEASED) {}

void rw_mutex::lock_read()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	i_lock_read();
}

bool rw_mutex::trylock_read()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	
	if(!can_start_reading(m_mode))
		return false;
	
	i_add_self();
	m_mode = MX_READ_ONLY;
	return true;
}

void rw_mutex::lock()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	i_lock();
}

bool rw_mutex::trylock()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	if(!can_start_writing(m_mode))
	{
		return false;
	}
	
	i_add_self();
	m_mode = MX_READ_WRITE;
	return true;
}

void rw_mutex::chlock_r()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	if(!i_has_self())
		throw std::runtime_error("A thread has unlocked a rw_mutex twice!");
	
	switch(m_mode) {
	case MX_READ_WRITE:
		m_mode = MX_READ_ONLY;
		break;
	case MX_READ_WRITE_AWAITING_WRITE:
		m_mode = MX_READ_ONLY_AWAITING_WRITE;
		break;
	case MX_READ_ONLY_AWAITING_WRITE:
	case MX_READ_ONLY:
		break;
	case MX_RELEASED:
	case MX_HANDOFF_WRITE:
	default:
		throw std::runtime_error("Unable to chlock_r. Lock was in an invalid state.");
		return;
	}
	
}

void rw_mutex::chlock_w()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	i_unlock();
	i_lock();
}

void rw_mutex::unlock()
{
	std::lock_guard<spinlock_mutex> lg(m_mutex);
	i_unlock();
}