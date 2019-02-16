// STL
#include <iostream>
#include <memory>
#include <fstream>

// Lua namespace
#include <state.h>

// fastcgi
#include <fcgi_config.h>
#include <fcgiapp.h>

// getpid
//#include <sys/types.h>
//#include <unistd.h>

// nanosleep
#include <time.h>

// Config
#include "settings.h"
#include "thread.h"
#include "statepool.h"
#include "monitor.h"

int main(int argc, char** argv) {
	std::unique_ptr<std::ofstream> logFile;
	//pid_t pid = getpid();
	
	if( !g_settings.LoadSettings((argc > 1 && argv[1]) ? argv[1] : "config.lua") )
	{
		std::cerr << "[PARENT] Unable to load luafcgid2 config!" << std::endl;
		return 1;
	}

	/* redirect stderr to logfile */
	if(!g_settings.m_logFile.empty())
	{
		logFile.reset(new std::ofstream(g_settings.m_logFile, std::ios_base::out | std::ios_base::app));
		if(*logFile) {
			std::cerr.rdbuf(logFile->rdbuf());
		} else {
			logFile.reset();
		}
	}
	
	if(!g_statepool.Start()) {
		std::cerr << "[PARENT] Unable to startup lua states pool!" << std::endl;
		return 1;
	}
	
	FCGX_Init();

	int sock = FCGX_OpenSocket(g_settings.m_listen.c_str(), -1);
	if (!sock) {
		std::cerr << "[PARENT] Unable to create FCGI socket!" << std::endl;
		return 1;
	}
	
	std::vector<std::unique_ptr<Thread>> threads;
	threads.reserve(g_settings.m_threadCount);
	
	for(int i = 0; i < g_settings.m_threadCount; ++i) {
		threads.emplace_back(new Thread(i, sock));
	}
	
	for(int i = 0; i < g_settings.m_threadCount; ++i) {
		threads[i]->Spawn();
	}
	
	timespec tv;
	tv.tv_sec = 3;
	tv.tv_nsec = 0;
	
	for (;;) {
		nanosleep(&tv, NULL);
	}

	return 0;
}