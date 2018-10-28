#include "thread.h"
#include "settings.h"
#include "statepool.h"
#include <fcgiapp.h>
#include <mutex>

std::mutex g_acceptMutex;

void RunThread(int tid, int sock)
{
	FCGX_Request request;
	FCGX_InitRequest(&request, sock, 0);
	
	LuaThreadCache cache;
	while(true)
	{
		g_acceptMutex.lock();
		FCGX_Accept_r(&request);
		g_acceptMutex.unlock();
		
		g_statepool.ExecMT(tid, request, cache);
		
		FCGX_Finish_r(&request);
	}
}

Thread::Thread(int thread_id, int sock) : m_thread_id(thread_id), m_sock(sock) {}
void Thread::Spawn() {
	if(m_thread.joinable())
		return;
	m_thread = std::thread(RunThread, m_thread_id, m_sock);
}