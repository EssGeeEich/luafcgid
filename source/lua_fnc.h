#ifndef LUA_FNC_H_INCLUDED
#define LUA_FNC_H_INCLUDED
#include "statepool.h"
#include "session.h"

struct LuaRequestData {
	LuaThreadCache* m_cache;
	FCGX_Request* m_request;
	LuaSessionInterface m_session;
};

void rawLuaHeader(LuaRequestData* reqData, std::string const& key, std::string const& val);
void SetupLuaFunctions(Lua::State& state, LuaRequestData& lrd);
#endif
