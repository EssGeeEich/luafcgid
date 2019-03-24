#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include <string>
#include <vector>
#include <mutex>
#include <ctime>
#include "state.h"

class Settings {
public:
	int m_threadCount;
	int m_states;
	int m_maxstates;
	int m_seek_retries;
	int m_headersize;
	int m_bodysize;
	int m_bodysectors;

	int m_fileInfoTime;
	bool m_useFileChecksum;

	std::string m_sessionName;
	int m_sessionTime;
	int m_sessionKeyLen;
	bool m_sessionCookieSecure;
	bool m_sessionCookieHttpOnly;
	
	int m_sessionIpScore;
	int m_sessionUserAgentScore;
	int m_sessionLanguageScore;
	int m_sessionTargetScore;

	std::string m_headers;
	std::string m_defaultHttpStatus;
	std::string m_defaultContentType;
	int m_maxPostSize;

	std::string m_listen;
	std::string m_logFile;
	std::string m_luaHeader;
	std::string m_luaEntrypoint;

	std::string m_luaLoadData;

	Lua::State m_luaState;

	void iPushValueTransfer(Lua::State& dest, int offset);
public:
	Settings();
	bool LoadSettings(std::string const& path);
	void TransferConfig(Lua::State& dest);
};

extern Settings g_settings;
extern std::mutex g_errormutex;

void LogError(std::string const&);
void LogError(char const*);
void gmtime_mx(std::time_t const& t, std::tm&);

#endif
