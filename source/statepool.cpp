#include "statepool.h"
#include "settings.h"
#include "lua_fnc.h"
#include "monitor.h"

#include <fstream>
#include <iostream>
#include <chrono>

// Lua status missing
static bool Handle404(std::string const& script, FCGX_Request& request)
{
	std::string str;
	str = "Status: 404 Not Found\r\nContent-Type: text-plain\r\n\r\nError: Page not found: ";
	str += script;
	str += ".";
	FCGX_PutStr(str.c_str(), str.length(), request.out);
	return false;
}

// Load the Lua script file
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
	{
		cache.chid = g_fmon.GetChangeId(cache.script);
		return true;
	}
	return false;
}

// Create the Lua status
static bool InitState(LuaState& lstate, LuaThreadCache const& cache)
{
	lstate.m_chid = 0;
	Lua::State& state = lstate.m_luaState;
	// Load cache.scriptData into lua state
	state = Lua::State::create();
	state.openlibs();
	state.luapp_register_metatables();
	
	g_settings.TransferConfig(state);
	
	if(state.loadbuffer(
		g_settings.m_luaLoadData.c_str(),
		g_settings.m_luaLoadData.size(),
		"head") != 0)
	{
		if(state.isstring(-1))
			LogError(state.tostdstring(-1));
		
		state.close();
		return false;
	}
	
	if(state.pcall() != 0)
	{
		if(state.isstring(-1))
			LogError(state.tostdstring(-1));
		
		state.close();
		return false;
	}
	
	if(state.loadbuffer(
		cache.scriptData.c_str(),
		cache.scriptData.size(),
		cache.script.c_str()) != 0)
	{
		if(state.isstring(-1))
			LogError(state.tostdstring(-1));
		
		state.close();
		return false;
	}
	
	if(state.pcall() != 0)
	{
		if(state.isstring(-1))
			LogError(state.tostdstring(-1));
		
		state.close();
		return false;
	}
	
	lstate.m_chid = cache.chid;
	return true;
}

bool LuaStatePool::ExecRequest(LuaState& luaState, int sid, int tid, FCGX_Request& request, LuaThreadCache& cache, clock::time_point start)
{
	Lua::State& state = luaState.m_luaState;
	cache.headers.clear();
	cache.getsBuffer.clear();
	cache.status = g_settings.m_defaultHttpStatus;
	cache.contentType = g_settings.m_defaultContentType;
	
	cache.headers.reserve(g_settings.m_headersize);
	cache.body.resize(g_settings.m_bodysectors);
	for(auto it = cache.body.begin(); it != cache.body.end(); ++it)
	{
		it->resize(0);
		it->reserve(g_settings.m_bodysize);
	}
	
	LuaRequestData lrd;
	lrd.m_cache = &cache;
	lrd.m_request = &request;
	
	SetupLuaFunctions(state, lrd);
	
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
	if(state.pcall() != 0)
	{
		if(state.isstring(-1))
			LogError(state.tostdstring(-1));
		
		return false;
	}
	
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
	for(auto it = lrd.m_cache->body.begin(); it != lrd.m_cache->body.end(); ++it)
	{
		FCGX_PutStr(it->c_str(), it->size(), lrd.m_request->out);
	}
	return true;
}

bool LuaStatePool::ExecMT(int tid, FCGX_Request& request, LuaThreadCache& cache)
{
	clock::time_point start = clock::now();
	
	char const* script = FCGX_GetParam("SCRIPT_FILENAME", request.envp);
	if(!script)
	{
		LogError("Invalid FCGI configuration: No SCRIPT_FILENAME variable.");
		return false;
	}
	
	if(!g_fmon.Initialized())
	{
		char const* root = FCGX_GetParam("DOCUMENT_ROOT", request.envp);
		if(!root)
		{
			LogError("Invalid FCGI configuration: No DOCUMENT_ROOT variable.");
			return false;
		}
		
		g_fmon.Init(root);
	}
	
	cache.script = FileMonitor::simplify(script);
	
	std::map<std::string,LuaStateContainer>::iterator selIterator;
	LuaState* selState = nullptr;
	int selStateNum = -1;
	std::unique_ptr<LuaState> ownState;
	bool justLoaded = false;
	
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
					return Handle404(cache.script, request);
				
				while(static_cast<int>(states.size()) < targetStates)
				{
					states.emplace_back();
					if(!InitState(states.back(), cache))
					{
						states.pop_back();
						return false;
					}
				}
				
				// Also grab the last generated state for future usage.
				selState = &(states.back());
				selStateNum = states.size();
				selState->m_inUse.test_and_set(std::memory_order_acquire);
				justLoaded = true;
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
				return Handle404(cache.script, request);
			
			LuaStateContainer& states = selIterator->second;
			int stateLimit = g_settings.m_maxstates;
			
			// Our container can fit another element (maxstates)
			if(static_cast<int>(states.size()) < stateLimit)
			{
				states.emplace_back();
				if(!InitState(states.back(), cache))
				{
					states.pop_back();
					return false;
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
					return false; // Error loading script
			}
			justLoaded = true;
		}
		
		selIterator = m_pool.end(); // Giving up the mutex. Don't use selIterator anymore.
	}
		
	// We need to check if the script is up-to-date.
	if(!justLoaded && selState->m_chid != g_fmon.GetChangeId(cache.script))
	{
		// Update now
		if(!InitData(cache))
			return Handle404(cache.script, request); // File has been deleted
		if(!InitState(*selState, cache))
			return false; // Error loading script
		selState->m_chid = cache.chid;
	}
	
	if(selState->m_chid < 0)
		return Handle404(cache.script, request); // Negative = 404
	
	// At this point, selState is finally valid, loaded and up-to-date.
	bool rv = false;
	try {
		// Elaborate the request here.
		rv = ExecRequest(*selState, selStateNum, tid, request, cache, start);
	}
	catch(std::exception& e) {
		LogError(e.what());
	}
	catch(...) {
		LogError("Unknown exception thrown.");
	}
	selState->m_inUse.clear(std::memory_order_release);
	return rv;
}


bool LuaStatePool::Start()
{
	// Just a placeholder for possible future implementation
	return true;
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

LuaStatePool g_statepool;