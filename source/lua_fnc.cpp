#include "lua_fnc.h"
#include "settings.h"

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
	LogError(data);
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

void SetupLuaFunctions(Lua::State& state, LuaRequestData& lrd)
{
	state.luapp_add_translated_function("Header", Lua::Transform(::luaHeader, lrd));
	state.luapp_add_translated_function("Send", Lua::Transform(::luaPuts, lrd));
	state.luapp_add_translated_function("Reset", Lua::Transform(::luaReset, lrd));
	state.luapp_add_translated_function("Log", Lua::Transform(::luaLog, lrd));
	state.luapp_add_translated_function("Receive", Lua::Transform(::luaGets, lrd));
	state.luapp_add_translated_function("RespStatus", Lua::Transform(::luaStatus, lrd));
	state.luapp_add_translated_function("RespContentType", Lua::Transform(::luaContentType, lrd));
	state.luapp_add_translated_function("ThreadData", Lua::Transform(::luaServerHealth, lrd));
}