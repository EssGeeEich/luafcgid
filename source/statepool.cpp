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
static bool InitData(LuaThreadCache& cache, std::size_t filesize, std::unique_ptr<std::ifstream>& f)
{
	// Load the script file
	if(!f)
		return false;
	cache.scriptData.resize(filesize);
	if(!f->read(&(cache.scriptData[0]), filesize))
		return false;
	
	return true;
}

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// Create the Lua status
static bool InitState(LuaState& lstate, LuaThreadCache const& cache, FileChangeData const& fcd)
{
	Lua::State& state = lstate.m_luaState;
	// Load cache.scriptData into lua state
	state = Lua::State::create();
	state.openlibs();
	
	// Make package.path and package.cpath localized and safer
	state.getglobal("package");
	{
		state.getfield(-1, "path");
		std::string path = state.tostdstring(-1);
		state.pop(1);
		
		replaceAll(path, ";./?.lua", "");
		replaceAll(path, ";./?/init.lua", "");
		path += ";" + cache.script.root() + "/?.lua";
		path += ";" + cache.script.root() + "/?/init.lua";
		path += ";" + cache.script.dir() + "/?.lua";
		path += ";" + cache.script.dir() + "/?/init.lua";
		
		state.pushstdstring(path);
		state.setfield(-2, "path");
	}
	{
		state.getfield(-1, "cpath");
		std::string cpath = state.tostdstring(-1);
		state.pop(1);
		
		// As a security measure, do not use relative paths for .so libs.
		replaceAll(cpath, ";./?.", ";" + cache.script.root() + "/?.");
		
		state.pushstdstring(cpath);
		state.setfield(-2, "cpath");
	}
	state.pop(1);
	
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
		cache.script.get().c_str()) != 0)
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
	
	lstate.m_chid = fcd;
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
			LogError(cache.script.get() + ": " + state.tostdstring(-1));
		
		state.gc(Lua::GC_COLLECT, 0);
		return false;
	}
	state.gc(Lua::GC_COLLECT, 0);
	
	int dur = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
	std::string sDur = std::to_string(dur);
	
	FCGX_PutStr("Status: ", 8, lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->status.c_str(), lrd.m_cache->status.size(), lrd.m_request->out);
	FCGX_PutStr("\r\nContent-Type: ", 16, lrd.m_request->out);
	FCGX_PutStr(lrd.m_cache->contentType.c_str(), lrd.m_cache->contentType.size(), lrd.m_request->out);
	FCGX_PutStr("\r\nX-ElapsedTime: ", 17, lrd.m_request->out);
	FCGX_PutStr(sDur.c_str(), sDur.size(), lrd.m_request->out);
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
	char const* root = FCGX_GetParam("DOCUMENT_ROOT", request.envp);
	if(!script)
	{
		LogError("Invalid FCGI configuration: No SCRIPT_FILENAME variable.");
		return false;
	}
	if(!root)
	{
		LogError("Invalid FCGI configuration: No DOCUMENT_ROOT variable.");
		return false;
	}
	
	cache.script = FileMonitor::simplify(script, root);
	
	std::map<std::string,LuaPool>::iterator selIterator;
	LuaState* selState = nullptr;
	int selStateNum = -1;
	std::unique_ptr<LuaState> ownState;
	
	// 0 -> Error, can't open file or such.
	// 1 -> Good, but our cache is outdated.
	// 2 -> Perfect. The file is up to date.
	
	auto LoadScript = [&](FileChangeData& chid, bool brandNew) -> int
	{
		std::unique_ptr<std::ifstream> f = FileMonitor::getFileForLoading(cache.script, chid, brandNew);
		if(!chid.m_exists
			|| (f && !InitData(cache, chid.m_filesize, f)))
			return 0; // Handle404(cache.script.get(), request);
		return (f) ? 1 : 2;
	};
	
	FileChangeData poolChangeData;
	{
		m_poolMutex.lock_read();
		std::lock_guard<rw_mutex> lg(m_poolMutex, std::adopt_lock);
		
		selIterator = m_pool.find(cache.script.get());
		if(selIterator == m_pool.end())
		{
			FileChangeData fcd;
			if(LoadScript(fcd, true) != 1)
				return Handle404(cache.script.get(), request);
			
			// Insert a new pair to std::map and populate it.
			m_poolMutex.chlock_w();
			
			auto pairResult = m_pool.emplace(std::make_pair(cache.script.get(),LuaStatePool::LuaPool()));
			selIterator = pairResult.first;
			selIterator->second.m_mostRecentChange = fcd;
			
			// This pair could be already populated if another thread ran chlock_w slightly before.
			LuaStateContainer& states = selIterator->second.m_states;
			int targetStates = g_settings.m_states;
			if(static_cast<int>(states.size()) < targetStates)
			{
				while(static_cast<int>(states.size()) < targetStates)
				{
					states.emplace_back();
					if(!InitState(states.back(), cache, fcd))
					{
						states.pop_back();
						return false;
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
			LuaStateContainer& states = selIterator->second.m_states;
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
			FileChangeData fcd;
			if(LoadScript(fcd, true) != 1)
				return Handle404(cache.script.get(), request);
			// No selState has been found.
			// As a last resort, create a new selState (and, if rules allow, store it)
			m_poolMutex.chlock_w();
			
			selIterator->second.m_mostRecentChange = fcd;
			LuaStateContainer& states = selIterator->second.m_states;
			int stateLimit = g_settings.m_maxstates;
			
			// Our container can fit another element (maxstates)
			if(static_cast<int>(states.size()) < stateLimit)
			{
				states.emplace_back();
				if(!InitState(states.back(), cache, fcd))
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
				if(InitState(*ownState, cache, fcd))
				{
					selState = ownState.get();
					selState->m_inUse.test_and_set(std::memory_order_acquire);
				}
				else
					return false; // Error loading script
			}
		}
		poolChangeData = selIterator->second.m_mostRecentChange;
		selIterator = m_pool.end(); // Giving up the mutex. Don't use selIterator anymore.
	}
	
	bool bForceReload = poolChangeData != selState->m_chid;
	switch(LoadScript(selState->m_chid, bForceReload))
	{
	case 0:
	default:
		return Handle404(cache.script.get(), request);
	case 1:
		// InitData has already been called.
		// All we need is already in the cache.
		if(!InitState(*selState, cache, selState->m_chid))
			return false;
		break;
	case 2:
		// No need to call anything.
		break;
	}
	
	// At this point, selState is finally valid, loaded and up-to-date.
	bool rv = false;
	try {
		// Elaborate the request here.
		rv = ExecRequest(*selState, selStateNum, tid, request, cache, start);
	}
	catch(std::exception& e) {
		LogError(cache.script.get() + ": " + e.what());
	}
	catch(...) {
		LogError(cache.script.get() + ": Unknown exception thrown.");
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
		data[it->first] = it->second.m_states.size();
	}
	
	return data;
}

LuaStatePool g_statepool;