#include "session.h"
#include "settings.h"
#include <mutex>
#include <stdexcept>

Session::Session(SessionManager*, std::string session, SessionDetectData const& sdd) :
	m_sessionKey(std::move(session)), m_sds(sdd) {}

Session::RealmIterator Session::GetRealms(std::string const& realm,
	bool bCreate)
{
	if(realm == "*")
		return Session::RealmIterator(m_realms.begin(), m_realms.end());
	if(bCreate)
	{
		auto endIter = m_realms.emplace(std::make_pair(realm, Session::Realm())).first;
		auto begIter = endIter++;
		return Session::RealmIterator(begIter, endIter);
	}

	auto endIter = m_realms.find(realm);
	if(endIter == m_realms.end())
		return Session::RealmIterator(endIter, endIter);
	auto begIter = endIter++;
	return Session::RealmIterator(begIter, endIter);
}

static inline std::time_t GetCurrentTimeT()
{
	return Session::expiration_clock::to_time_t(Session::expiration_clock::now());
}

void Session::Touch()
{
	m_expiration = GetCurrentTimeT() + g_settings.m_sessionTime;
}

bool Session::IsValid()
{
	return m_expiration.load() >= GetCurrentTimeT();
}

void Session::Start()
{
	Touch();
}

bool Session::HasRealm(std::string const& realm)
{
	Touch();

	m_mutex.lock_read();
	std::lock_guard<rw_mutex> mx(m_mutex, std::adopt_lock);
	return m_realms.find(realm) != m_realms.end();
}

void Session::Clear(std::string const& realm)
{
	Touch();

	std::lock_guard<rw_mutex> mx(m_mutex);
	Session::RealmIterator realms = GetRealms(realm, false);
	std::size_t c = 0;
	for(auto it = realms.begin(); it != realms.end(); ++it, ++c)
	{
		it->second.clear();
	}
	if(c == m_realms.size())
		m_realms.clear();
}

void Session::Delete()
{
	m_expiration = 0;
}

bool Session::SetVar(std::string const& realm, std::string const& key, Lua::Variable* var)
{
	Touch();

	Lua::Bool* boolVar = nullptr;
	Lua::Number* numVar = nullptr;
	Lua::String* strVar = nullptr;

	Lua::ReturnValues vals;
	bool has_vals = false;
	
	if(!var) {}
	else if(var->IsBool(boolVar))
	{
		vals = Lua::Return(boolVar->cget());
		has_vals = true;
	}
	else if(var->IsNumber(numVar))
	{
		vals = Lua::Return(numVar->cget());
		has_vals = true;
	}
	else if(var->IsString(strVar))
	{
		vals = Lua::Return(strVar->cget());
		has_vals = true;
	}

	std::lock_guard<rw_mutex> mx(m_mutex);
	Session::RealmIterator realms = GetRealms(realm, true);
	if(has_vals)
	{
		for(auto it = realms.begin(); it != realms.end(); ++it)
		{
			it->second[key] = vals;
		}
	}
	else
	{
		for(auto it = realms.begin(); it != realms.end(); ++it)
		{
			auto dlk = it->second.find(key);
			if(dlk != it->second.end())
				it->second.erase(dlk);
		}
	}
	return has_vals || (!var) || var->IsNil() || (var->GetType() == Lua::TP_NONE);
}

Lua::ReturnValues Session::GetVar(std::string const& realm, std::string const& key)
{
	Touch();

	m_mutex.lock_read();
	std::lock_guard<rw_mutex> mx(m_mutex, std::adopt_lock);
	Session::RealmIterator realms = GetRealms(realm, false);
	for(auto it = realms.begin(); it != realms.end(); ++it)
	{
		auto itk = it->second.find(key);
		if(itk != it->second.end())
			return itk->second;
	}
	return Lua::Return();
}

// LuaSessionInterface
LuaSessionInterface::LuaSessionInterface() : m_manager(nullptr), m_realSession(nullptr) {}

bool LuaSessionInterface::getCookieString(std::string& s) const
{
	if(!m_realSession || !m_realSession->IsValid())
	{
		// We didn't have a session key to begin with.
		if(m_sdd.m_sessionKey.empty())
			return false;
		
		// We deleted the session.
		s = g_settings.m_sessionName + "=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
		return true;
	}
	
	// Calc the expiration...
	std::tm gmt_time;
	std::time_t epoch_time = m_realSession->m_expiration.load();
	gmtime_mx(epoch_time, gmt_time);
	
	char cookie_str_fmt[64];
	std::strftime(cookie_str_fmt, sizeof(cookie_str_fmt), "%a, %d %b %Y %H:%M:%S GMT", &gmt_time);
	
	// We have a session!
	s = g_settings.m_sessionName + "=" + m_realSession->m_sessionKey + "; Expires="
		+ cookie_str_fmt + ";";
		
	if(g_settings.m_sessionCookieHttpOnly)
		s.append(" HttpOnly;");
	if(g_settings.m_sessionCookieSecure)
		s.append(" Secure;");
	return true;
}

void LuaSessionInterface::Init(SessionManager& manager, SessionDetectData const& sdd)
{
	m_manager = &manager;
	m_realSession = manager.findSession(sdd);
	m_sdd = sdd;
}

void LuaSessionInterface::DeleteSessionTicket()
{
	if(!m_realSession)
		return;
	if(!m_manager)
		throw std::runtime_error("No session manager in LuaSessionInterface!");
	m_manager->DeleteSession(m_realSession);
	m_realSession = nullptr;
}

void LuaSessionInterface::CreateNewSessionTicket()
{
	if(m_realSession)
		return;
	if(!m_manager)
		throw std::runtime_error("No session manager in LuaSessionInterface!");
	m_realSession = m_manager->CreateSession(m_sdd);
}

bool LuaSessionInterface::read() const
{
	return !!m_realSession;
}

void LuaSessionInterface::write()
{
	CreateNewSessionTicket();
}

void LuaSessionInterface::Start()
{
	write();
	m_realSession->Start();
}

void LuaSessionInterface::Delete()
{
	if(!read())
		return;
	m_realSession->Delete();
	DeleteSessionTicket();
}

bool LuaSessionInterface::HasRealm(std::string const& realm)
{
	if(!read())
		return false;
	return m_realSession->HasRealm(realm);
}

void LuaSessionInterface::Clear(std::string const& realm)
{
	if(!read())
		return;
	m_realSession->Clear(realm);
}

bool LuaSessionInterface::SetVar(std::string const& realm, std::string const& key, Lua::Variable* val)
{
	write();
	return m_realSession->SetVar(realm, key, val);
}

Lua::ReturnValues LuaSessionInterface::GetVar(std::string const& realm, std::string const& key)
{
	if(!read())
		return Lua::Return();
	return m_realSession->GetVar(realm, key);
}

// SessionManager
SessionManager g_sessions;

Session* SessionManager::CreateSession(SessionDetectData const& sdd)
{
	std::lock_guard<rw_mutex> mx(m_mutex);
	std::string skey;
	
	while(true)
	{
		SessionManager::CreateSessionKey(skey);
		auto it = m_sessions.emplace(std::piecewise_construct, std::make_tuple(skey), std::make_tuple(this,skey,sdd));
		if(it.second == true)
			return &(it.first->second);
	}
}

void SessionManager::DeleteSession(Session* session)
{
	if(!session)
		return;
	std::lock_guard<rw_mutex> mx(m_mutex);
	
	auto it = m_sessions.find(session->m_sessionKey);
	if(it == m_sessions.end())
		return;
	m_sessions.erase(it);
}

Session* SessionManager::findSession(SessionDetectData const& sdd)
{
	auto it = m_sessions.begin();
	
	{
		m_mutex.lock_read();
		std::lock_guard<rw_mutex> mx(m_mutex, std::adopt_lock);
		
		it = m_sessions.find(sdd.m_sessionKey);
		if(it == m_sessions.end())
			return nullptr;
	}
	
	it->second.m_mutex.lock_read();
	std::lock_guard<rw_mutex> mx(it->second.m_mutex, std::adopt_lock);
	if(!sessionMatches(sdd, it->second.m_sds, true))
		return nullptr; // Safety measure. The user identity doesn't seem to match!
	
	it->second.m_mutex.chlock_w();
	it->second.m_sds = sdd;
	return &(it->second);
}

#include "randutils.hpp"
typedef randutils::mt19937_rng random_generator;
typedef std::uniform_int_distribution<char> random_distribution;

static char const g_selCharacters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
static int const g_sc_first = 0;
static int const g_sc_last = (sizeof(g_selCharacters) / sizeof(*g_selCharacters))-2;

void SessionManager::CreateSessionKey(std::string& result)
{
	int const klen = g_settings.m_sessionKeyLen;
	
	random_generator generator;
	result.resize(klen);
	
	for(int i = 0; i < klen; ++i)
	{
		result[i] = g_selCharacters[generator.uniform(g_sc_first, g_sc_last)];
	}
}