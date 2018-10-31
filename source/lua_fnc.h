#include "statepool.h"

struct LuaRequestData {
	LuaThreadCache* m_cache;
	FCGX_Request* m_request;
};

void SetupLuaFunctions(Lua::State& state, LuaRequestData& lrd);