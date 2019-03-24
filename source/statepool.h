#ifndef STATEPOOL_H_INCLUDED
#define STATEPOOL_H_INCLUDED
#include <list>
#include <vector>
#include <atomic>
#include <string>
#include <map>
#include <fcgiapp.h>
#include "rw_mutex.h"
#include "state.h"
#include "monitor.h"

struct LuaState {
	std::atomic_flag m_inUse;
	FileChangeData m_chid;
	Lua::State m_luaState;
};

struct LuaThreadCache {
	SimplifiedPath script;
	std::string scriptData;
	std::string headers;
	std::vector<std::string> body;
	std::string getsBuffer;
	std::string status;
	std::string contentType;
};

class LuaStatePool {
	typedef std::list<LuaState> LuaStateContainer;
	typedef std::chrono::high_resolution_clock clock;

	struct LuaPool {
		LuaStateContainer m_states;
		FileChangeData m_mostRecentChange;
	};

	rw_mutex m_poolMutex;
	std::map<std::string,LuaPool> m_pool;

	bool ExecRequest(LuaState& state, int sid, int tid, FCGX_Request& request, LuaThreadCache& cache, clock::time_point start);
public:
	bool Start();
	bool ExecMT(int tid, FCGX_Request& request, LuaThreadCache& cache);
	std::map<std::string, int> ServerInfo();
};

extern LuaStatePool g_statepool;
#endif
