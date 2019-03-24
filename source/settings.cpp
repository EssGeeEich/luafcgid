#include "settings.h"

// STL
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <iostream>

static std::string g_luaHeader = R"====(
LUAFCGID=true
LUAFCGID_VERSION=2
lf={}
function lf.serialize(a)local b=""if type(a)=="table"then local c={}for d,e in pairs(a)do if type(d)=="number"then d=tostring(d)elseif type(d)=="string"then d=string.format("[%q]",d)end;table.insert(c,string.format("%s = %s",d,lf.serialize(e)))end;b='{'..table.concat(c,",")..'}'elseif type(a)=="number"then b=tostring(a)elseif type(a)=="string"then b=string.format("%q",a)elseif type(a)=="boolean"then b=a and"true"or"false"end;return b end
function lf.urlencode(a)if a and#a>0 then a=string.gsub(a,"([^%w ])",function(b)return string.format("%%%02X",string.byte(b))end)a=string.gsub(a," ","+")end;return a end
function lf.urldecode(a)if a and#a>0 then a=string.gsub(a,"+"," ")a=string.gsub(a,"%%(%x%x)",function(b)return string.char(tonumber(b,16))end)end;return a end
function lf.parse_pair(b)local c,d;if b and#b>0 then _,_,c,d=string.find(b,"([^=]*)=([^=]*)")if not d then d=""end end;return lf.urldecode(c),lf.urldecode(d)end
function lf.parse(a)local b={}for c in string.gmatch(a,"[^&]*")do if c and#c>0 then local d,e=lf.parse_pair(c)if b[d]then if type(b[d])~="table"then b[d]={b[d]}end;table.insert(b[d],e)else b[d]=e end end end;return b end

Response={
[100]="100 Continue",[101]="101 Switching Protocols",[102]="102 Processing",[103]="103 Early Hints",
[200]="200 OK",[201]="201 Created",[202]="202 Accepted",[203]="203 Non-Authoritative Information",[204]="204 No Content",[205]="205 Reset Content",[206]="206 Partial Content",[207]="207 Multi-Status",[208]="208 Already Reported",[226]="226 IM Used",
[300]="300 Multiple Choices",[301]="301 Moved Permanently",[302]="302 Found",[303]="303 See Other",[304]="304 Not Modified",[305]="305 Use Proxy",[306]="306 Switch Proxy",[307]="307 Temporary Redirect",[308]="308 Permanent Redirect",
[400]="400 Bad Request",[401]="401 Unauthorized",[402]="402 Payment Required",[403]="403 Forbidden",[404]="404 Not Found",[405]="405 Method Not Allowed",[406]="406 Not Acceptable",[407]="407 Proxy Authentication Required",[408]="408 Request Timeout",[409]="409 Conflict",[410]="410 Gone",[411]="411 Length Required",[412]="412 Precondition Failed",[413]="413 Payload Too Large",[414]="414 URI Too Long",[415]="415 Unsupported Media Type",
[416]="416 Range Not Satisfiable",[417]="417 Expectation Failed",[418]="418 I'm a teapot",[421]="421 Misdirected Request",[422]="422 Unprocessable Entity",[423]="423 Locked",[424]="424 Failed Dependency",[426]="426 Upgrade Required",[428]="428 Precondition Required",[429]="429 Too Many Requests",[431]="431 Request Header Fields Too Large",[451]="451 Unavailable For Legal Reasons",
[500]="500 Internal Server Error",[501]="501 Not Implemented",[502]="502 Bad Gateway",[503]="503 Service Unavailable",[504]="504 Gateway Timeout",[505]="505 HTTP Version Not Supported",[506]="506 Variant Also Negotiates",[507]="507 Insufficient Storage",[508]="508 Loop Detected",[510]="510 Not Extended",[511]="511 Network Authentication Required"
}
)====";

void BindBool(Lua::State& s, const char* variable, bool& def_val) {
	if(s.getglobal(variable) == Lua::TP_BOOL) {
		def_val = s.toboolean(-1);
	}
	s.pop(1);
}

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
	m_bodysize(2048),
	m_bodysectors(4),
	m_fileInfoTime(5000),
	m_useFileChecksum(true),
	m_sessionName("XLuaSession"),
	m_sessionTime(3600),
	m_sessionKeyLen(24),
	
	m_sessionIpScore(3),
	m_sessionUserAgentScore(2),
	m_sessionLanguageScore(1),
	m_sessionTargetScore(3),
	
	m_headers("X-Powered-By: luafcgid2\r\n"),
	m_defaultHttpStatus("200 OK"),
	m_defaultContentType("text/html"),
	m_maxPostSize(1024 * 4096),
	m_listen("/var/tmp/luafcgid2.sock"),
	m_logFile("/var/log/luafcgid2/luafcgid2.log"),
	m_luaEntrypoint("main")
{}


void Settings::iPushValueTransfer(Lua::State& dest, int offset)
{
	// Pop a value from m_luaState
	// Push a value to dest
	Lua::Type t = m_luaState.type(offset);

	switch(t)
	{
	case Lua::TP_NONE:
	case Lua::TP_NIL:
	default:
		dest.pushnil();
		break;
	case Lua::TP_BOOL:
		dest.pushboolean(m_luaState.toboolean(offset));
		break;
	case Lua::TP_NUMBER:
		if(m_luaState.isinteger(offset))
			dest.pushinteger(m_luaState.tointeger(offset));
		else
			dest.pushnumber(m_luaState.tonumber(offset));
		break;
	case Lua::TP_STRING:
		dest.pushstdstring(m_luaState.tostdstring(offset));
		break;
	case Lua::TP_TABLE:
		{
			dest.newtable();
			m_luaState.pushnil();
			if(offset < 0)
				--offset;
			while(m_luaState.next(offset) != 0)
			{
				iPushValueTransfer(dest, -2);
				iPushValueTransfer(dest, -1);
				dest.rawset(-3);
				m_luaState.pop(1);
			}
			++offset;
		}
		break;
	}
}

// setup LuaConfig.* and Config.*
std::mutex g_transferMutex;
void Settings::TransferConfig(Lua::State& dest)
{
	std::lock_guard<std::mutex> g(g_transferMutex);

	m_luaState.getglobal("Config");
	iPushValueTransfer(dest, -1);
	m_luaState.pop(1);
	dest.setglobal("Config");
}

bool Settings::LoadSettings(std::string const& path)
{
	m_luaState = Lua::State::create();
	if(m_luaState.loadfile(path.c_str()) == LUA_OK && m_luaState.pcall() == LUA_OK) {
		BindNumber(m_luaState, "WorkerThreads", m_threadCount);
		BindNumber(m_luaState, "LuaStates", m_states);
		BindNumber(m_luaState, "LuaMaxStates", m_maxstates);
		BindNumber(m_luaState, "LuaMaxSearchRetries", m_seek_retries);
		BindNumber(m_luaState, "HeadersSize", m_headersize);
		BindNumber(m_luaState, "BodySize", m_bodysize);
		BindNumber(m_luaState, "BodySectors", m_bodysectors);
		BindNumber(m_luaState, "MinFileInfoTime", m_fileInfoTime);
		BindBool  (m_luaState, "UseFileChecksum", m_useFileChecksum);
		BindString(m_luaState, "SessionName", m_sessionName);
		BindNumber(m_luaState, "SessionTime", m_sessionTime);
		BindNumber(m_luaState, "SessionKeyLen", m_sessionKeyLen);
		BindNumber(m_luaState, "SessionIpScore", m_sessionIpScore);
		BindNumber(m_luaState, "SessionUserAgentScore", m_sessionUserAgentScore);
		BindNumber(m_luaState, "SessionLanguageScore", m_sessionLanguageScore);
		BindNumber(m_luaState, "SessionTargetScore", m_sessionTargetScore);
		BindString(m_luaState, "DefaultHeaders", m_headers);
		BindString(m_luaState, "DefaultHttpStatus", m_defaultHttpStatus);
		BindString(m_luaState, "DefaultContentType", m_defaultContentType);
		BindNumber(m_luaState, "MaxPostSize", m_maxPostSize);
		BindString(m_luaState, "LogFilePath", m_logFile);
		BindString(m_luaState, "Listen", m_listen);
		BindString(m_luaState, "StartupScript", m_luaHeader);
		BindString(m_luaState, "Entrypoint", m_luaEntrypoint);
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
	if(m_bodysectors < 0)
		m_bodysectors = 0;
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

void LogError(std::string const& s)
{
	std::lock_guard<std::mutex> lg(g_errormutex);
	std::cerr << s << std::endl;
}

void LogError(char const* s)
{
	std::lock_guard<std::mutex> lg(g_errormutex);
	std::cerr << s << std::endl;
}

Settings g_settings;
std::mutex g_errormutex;

static std::mutex g_gmtime_mx;
void gmtime_mx(std::time_t const& t, std::tm& r)
{
	std::lock_guard<std::mutex> lg(g_gmtime_mx);
	r = *std::gmtime(&t);
}
