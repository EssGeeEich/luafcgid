#include "statepool.h"
#include "settings.h"
#include <fstream>
#include <iostream>
#include <chrono>

bool LuaStatePool::Start()
{
	// Just a placeholder for possible future implementation
	return true;
}

static bool InitData(LuaThreadCache& cache)
{
	// Load the script file
	std::ifstream myScript(cache.script, std::ios::ate | std::ios::binary);
	if(!myScript) {
		return false;
	}
	std::streamsize size = myScript.tellg();
	if(size <= 0) {
		return false;
	}
	cache.scriptData.resize(size);
	myScript.seekg(0, std::ios::beg);
	if(myScript.read(&(cache.scriptData[0]), size))
		return true;
	return false;
}

static bool InitState(LuaState& lstate, LuaThreadCache const& cache)
{
	Lua::State& state = lstate.m_luaState;
	// Load cache.scriptData into lua state
	state = Lua::State::create();
	state.openlibs();
	state.luapp_register_metatables();
	
	if(state.loadbuffer(
		g_settings.m_luaLoadData.c_str(),
		g_settings.m_luaLoadData.size(),
		"head") != 0)
	{
		if(state.isstring(-1))
		{
			std::lock_guard<std::mutex> em(g_errormutex);
			std::cerr << state.tostdstring(-1) << std::endl;
		}
		state.close();
		return false;
	}
	
	if(state.pcall() != 0)
	{
		if(state.isstring(-1))
		{
			std::lock_guard<std::mutex> em(g_errormutex);
			std::cerr << state.tostdstring(-1) << std::endl;
		}
		state.close();
		return false;
	}
	
	if(state.loadbuffer(
		cache.scriptData.c_str(),
		cache.scriptData.size(),
		cache.script.c_str()) != 0)
	{
		if(state.isstring(-1))
		{
			std::lock_guard<std::mutex> em(g_errormutex);
			std::cerr << state.tostdstring(-1) << std::endl;
		}
		state.close();
		return false;
	}
	
	if(state.pcall() != 0)
	{
		if(state.isstring(-1))
		{
			std::lock_guard<std::mutex> em(g_errormutex);
			std::cerr << state.tostdstring(-1) << std::endl;
		}
		state.close();
		return false;
	}
	
	return true;
}


struct LuaRequestData {
	LuaThreadCache* m_cache;
	FCGX_Request* m_request;
};

static void luaHeader(LuaRequestData reqData, std::string const& key, Lua::Arg<std::string> const& val)
{
	reqData.m_cache->headers.append(key);
	if(val) {
		reqData.m_cache->headers.append(": ");
		reqData.m_cache->headers.append(*val);
	}
	reqData.m_cache->headers.append("\r\n");
}

static void luaPuts(LuaRequestData reqData, std::string const& data)
{
	reqData.m_cache->body.append(data);
}

static void luaReset(LuaRequestData reqData)
{
	reqData.m_cache->headers.clear();
	reqData.m_cache->body.clear();
}

static void luaLog(LuaRequestData, std::string const& data)
{
	std::lock_guard<std::mutex> m(g_errormutex);
	std::cerr << data << std::endl;
}

static std::string luaGets(LuaRequestData reqData)
{
	int const chunk_size = 1024;
	reqData.m_cache->getsBuffer.clear();
	reqData.m_cache->getsBuffer.resize(chunk_size);
	
	std::string getsData;
	int len = 0;
	do {
		len = FCGX_GetStr(&(reqData.m_cache->getsBuffer[0]), chunk_size, reqData.m_request->in);
		if(len > 0)
			getsData.append(&(reqData.m_cache->getsBuffer[0]), static_cast<std::size_t>(len));
	} while(len == chunk_size);
	return getsData;
}

static void luaStatus(LuaRequestData reqData, std::string const& data)
{
	reqData.m_cache->status = data;
}

static void luaContentType(LuaRequestData reqData, std::string const& data)
{
	reqData.m_cache->contentType = data;
}

static Lua::Map<int> luaServerHealth(LuaRequestData)
{
	Lua::Map<int> d;
	d.m_data = g_statepool.ServerInfo();
	return d;
}

std::map<std::string, int> LuaStatePool::ServerInfo()
{
	m_poolMutex.lock_read();
	std::lock_guard<rw_mutex> lg(m_poolMutex, std::adopt_lock);
	std::map<std::string, int> data;
	
	for(auto it = m_pool.begin(); it != m_pool.end(); ++it)
	{
		data[it->first] = it->second.size();
	}
	
	return data;
}

void LuaStatePool::ExecRequest(LuaState& luaState, int sid, int tid, FCGX_Request& request, LuaThreadCache& cache, clock::time_point start)
{
	Lua::State& state = luaState.m_luaState;
	cache.headers.clear();
	cache.body.clear();
	cache.getsBuffer.clear();
	cache.status = g_settings.m_defaultHttpStatus;
	cache.contentType = g_settings.m_defaultContentType;
	
	cache.headers.reserve(g_settings.m_headersize);
	cache.body.reserve(g_settings.m_bodysize);
	
	LuaRequestData lrd;
	lrd.m_cache = &cache;
	lrd.m_request = &request;
	
	state.luapp_add_translated_function("Header", Lua::Transform(::luaHeader, lrd));
	state.luapp_add_translated_function("Send", Lua::Transform(::luaPuts, lrd));
	state.luapp_add_translated_function("Reset", Lua::Transform(::luaReset, lrd));
	state.luapp_add_translated_function("Log", Lua::Transform(::luaLog, lrd));
	state.luapp_add_translated_function("Receive", Lua::Transform(::luaGets, lrd));
	state.luapp_add_translated_function("RespStatus", Lua::Transform(::luaStatus, lrd));
	state.luapp_add_translated_function("RespContentType", Lua::Transform(::luaContentType, lrd));
	state.luapp_add_translated_function("ThreadData", Lua::Transform(::luaServerHealth, lrd));
	
	state.newtable();
	char** p = lrd.m_request->envp;
	while(p && *p) {
		char* v = strchr(*p, '=');
		if(v) {
			state.pushlstring(*p, v - *p);
			state.pushstring(++v);
			state.settable(-3);
		}
		++p;
	}
	state.setglobal("Env");
	
	state.newtable();
		state.pushstring("State");
		state.pushinteger(sid);
		state.settable(-3);
		
		state.pushstring("Thread");
		state.pushinteger(tid);
		state.settable(-3);
	state.setglobal("Info");
	
	state.getglobal(g_settings.m_luaEntrypoint.c_str());
	state.pcall();
	
	int dur = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
	lrd.m_cache->script.assign(std::to_string(dur));
	
	FCGX_PutStr("Status: ", 8, lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->status.c_str(), lrd.m_cache->status.size(), lrd.m_request->out);
	FCGX_PutStr("\r\nContent-Type: ", 16, lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->contentType.c_str(), lrd.m_cache->contentType.size(), lrd.m_request->out);
	FCGX_PutStr("\r\nX-ElapsedTime: ", 17, lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->script.c_str(), lrd.m_cache->script.size(), lrd.m_request->out);
	FCGX_PutStr("\r\n", 2, lrd.m_request->out);
	FCGX_PutStr(g_settings.m_headers.c_str(), g_settings.m_headers.size(), lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->headers.c_str(), lrd.m_cache->headers.size(), lrd.m_request->out);
	FCGX_PutStr("\r\n", 2, lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->body.c_str(), lrd.m_cache->body.size(), lrd.m_request->out);
}

int LuaStatePool::ExecMT(int tid, FCGX_Request& request, LuaThreadCache& cache)
{
	clock::time_point start = clock::now();
	
	char const* script = FCGX_GetParam("SCRIPT_FILENAME", request.envp);
	if(!script)
		return 500;
	
	cache.script = script;
	
	std::map<std::string,LuaStateContainer>::iterator selIterator;
	LuaState* selState = nullptr;
	int selStateNum = -1;
	std::unique_ptr<LuaState> ownState;
	
	{
		m_poolMutex.lock_read();
		std::lock_guard<rw_mutex> lg(m_poolMutex, std::adopt_lock);
		
		selIterator = m_pool.find(cache.script);
		if(selIterator == m_pool.end())
		{
			// Insert a new pair to std::map and populate it.
			m_poolMutex.chlock_w();
			
			auto pairResult = m_pool.emplace(std::make_pair(cache.script,LuaStateContainer()));
			selIterator = pairResult.first;
			
			// This pair could be already populated if another thread ran chlock_w slightly before.
			LuaStateContainer& states = selIterator->second;
			int targetStates = g_settings.m_states;
			if(static_cast<int>(states.size()) < targetStates)
			{
				if(!InitData(cache))
					return 404;
				
				while(static_cast<int>(states.size()) < targetStates)
				{
					states.emplace_back();
					if(!InitState(states.back(), cache))
					{
						states.pop_back();
						return 500;
					}
				}
				
				// Also grab the last generated state for future usage.
				selState = &(states.back());
				selStateNum = states.size();
				selState->m_inUse.test_and_set(std::memory_order_acquire);
			}
			
			m_poolMutex.chlock_r();
		}
		
		// m_poolMutex is in read state
		// selIterator points to a good pair
		// selState might already point to a good state.
		
		if(!selState)
		{
			// We need to find a selState.
			
			// Try finding a good state for the required script
			LuaStateContainer& states = selIterator->second;
			int max_retries = g_settings.m_seek_retries;
			
			for(int i = 0; !selState && (i < max_retries); ++i)
			{
				if(i > 0)
					std::this_thread::yield();
				
				int x = 0;
				for(auto it = states.begin(); it != states.end(); ++it, ++x)
				{
					if(!it->m_inUse.test_and_set(std::memory_order_acquire))
					{
						selState = &(*it);
						selStateNum = x;
						break;
					}
				}
			}
		}
			
		if(!selState)
		{
			// No selState has been found.
			// As a last resort, create a new selState (and, if rules allow, store it)
			m_poolMutex.chlock_w();
			
			if(!InitData(cache))
				return 404;
			
			LuaStateContainer& states = selIterator->second;
			int stateLimit = g_settings.m_maxstates;
			
			// Our container can fit another element (maxstates)
			if(static_cast<int>(states.size()) < stateLimit)
			{
				states.emplace_back();
				if(!InitState(states.back(), cache))
				{
					states.pop_back();
					return 500;
				}
				selState = &(states.back());
				selStateNum = states.size();
				selState->m_inUse.test_and_set(std::memory_order_acquire);
			}
			else
			{
				// We have already reached maxstates.
				// Create a temporary LuaState.
				ownState.reset(new LuaState);
				if(InitState(*ownState, cache))
				{
					selState = ownState.get();
					selState->m_inUse.test_and_set(std::memory_order_acquire);
				}
				else
					return 500; // Error loading script
			}
		}
	}
	// At this point, selState is finally valid and loaded.
	
	try {
		// Elaborate the request here.
		ExecRequest(*selState, selStateNum, tid, request, cache, start);
	}
	catch(...) {
		selState->m_inUse.clear(std::memory_order_release);
		return 500;
	}
	selState->m_inUse.clear(std::memory_order_release);
	return 0;
}

LuaStatePool g_statepool;