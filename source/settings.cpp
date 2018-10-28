#include "settings.h"

// STL
#include <stdexcept>
#include <algorithm>
#include <fstream>

// LuaPP
#include <state.h>

static std::string g_luaHeader = R"====(

LUAFCGID=true
LUAFCGID_VERSION=2
lf={}
function lf.serialize(a)local b=""if type(a)=="table"then local c={}for d,e in pairs(a)do if type(d)=="number"then d=tostring(d)elseif type(d)=="string"then d=string.format("[%q]",d)end;table.insert(c,string.format("%s = %s",d,lf.serialize(e)))end;b='{'..table.concat(c,",")..'}'elseif type(a)=="number"then b=tostring(a)elseif type(a)=="string"then b=string.format("%q",a)elseif type(a)=="boolean"then b=a and"true"or"false"end;return b end
function lf.urlencode(a)if a and#a>0 then a=string.gsub(a,"([^%w ])",function(b)return string.format("%%%02X",string.byte(b))end)a=string.gsub(a," ","+")end;return a end
function lf.urldecode(a)if a and#a>0 then a=string.gsub(a,"+"," ")a=string.gsub(a,"%%(%x%x)",function(b)return string.char(tonumber(b,16))end)end;return a end
local function a(b)local c,d;if b and#b>0 then _,_,c,d=string.find(b,"([^=]*)=([^=]*)")if not d then d=""end end;return lf.urldecode(c),lf.urldecode(d)end
function lf.parse(a)local b={}for c in string.gmatch(a,"[^&]*")do if c and#c>0 then local d,e=parse_pair(c)if b[d]then if type(b[d])~="table"then b[d]={b[d]}end;table.insert(b[d],e)else b[d]=e end end end;return b end
return lf

)====";

void BindNumber(Lua::State& s, const char* variable, int& def_val) {
	if(s.getglobal(variable) == Lua::TP_NUMBER) {
		def_val = s.tonumber(-1);
	}
	s.pop(1);
}

void BindString(Lua::State& s, const char* variable, std::string& def_val) {
	if(s.getglobal(variable) == Lua::TP_STRING) {
		def_val = s.tostdstring(-1);
	}
	s.pop(1);
}

Settings::Settings() :
	m_threadCount(4),
	m_states(3),
	m_maxstates(5),
	m_seek_retries(3),
	m_headersize(256),
	m_bodysize(8192),
	m_headers("X-Powered-By: luafcgid2\r\n"),
	m_defaultHttpStatus("200 OK"),
	m_defaultContentType("text/html"),
	m_maxPostSize(1024 * 4096),
	m_listen("/var/tmp/luafcgid2.sock"),
	m_logFile("/var/log/luafcgid2/luafcgid2.log"),
	m_luaEntrypoint("main")
{}

bool Settings::LoadSettings(std::string const& path)
{
	Lua::State state = Lua::State::create();
	if(state.loadfile(path.c_str()) == LUA_OK && state.pcall() == LUA_OK) {
		BindNumber(state, "threads", m_threadCount);
		BindNumber(state, "states", m_states);
		BindNumber(state, "maxstates", m_maxstates);
		BindNumber(state, "retries", m_seek_retries);
		BindNumber(state, "headersize", m_headersize);
		BindNumber(state, "bodysize", m_bodysize);
		BindString(state, "headers", m_headers);
		BindString(state, "httpstatus", m_defaultHttpStatus);
		BindString(state, "contenttype", m_defaultContentType);
		BindNumber(state, "maxpost", m_maxPostSize);
		BindString(state, "logfile", m_logFile);
		BindString(state, "listen", m_listen);
		BindString(state, "script", m_luaHeader);
		BindString(state, "entrypoint", m_luaEntrypoint);
	}
	
	if(m_threadCount < 1)
		m_threadCount = 1;
	if(m_states < 1)
		m_states = 1;
	if(m_maxstates < 1)
		m_maxstates = 1;
	if(m_seek_retries < 1)
		m_seek_retries = 1;
	if(m_headersize < 0)
		m_headersize = 0;
	if(m_bodysize < 0)
		m_bodysize = 0;
	if(m_maxPostSize < 0)
		m_maxPostSize = 0;
	
	{
		m_luaLoadData = g_luaHeader;
		if(!m_luaHeader.empty())
		{
			std::ifstream myScript(m_luaHeader, std::ios::ate | std::ios::binary);
			if(myScript)
			{
				std::streamsize size = myScript.tellg();
				if(size > 0)
				{
					m_luaLoadData.resize(size + g_luaHeader.size());
					myScript.seekg(0, std::ios::beg);
					if(!myScript.read(&(m_luaLoadData[g_luaHeader.size()]), size))
						m_luaLoadData = g_luaHeader;
				}
			}
		}
	}
		
	return true;
}

Settings g_settings;
std::mutex g_errormutex;