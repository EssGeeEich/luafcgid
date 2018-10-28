#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <string>
#include <vector>
#include <thread>

class Thread {
	int const m_thread_id;
	int const m_sock;
	std::thread m_thread;
	
	Thread(Thread const&) =delete;
	Thread& operator= (Thread const&) =delete;
public:
	Thread(int thread_id, int sock);
	void Spawn();
};

#endif