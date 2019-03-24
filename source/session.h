#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED
#include <string>
#include <chrono>
#include <ctime>
#include "state.h"
#include "rw_mutex.h"
#include "settings.h"

struct SessionDetectData {
	inline SessionDetectData() :
		m_sessionKey(),
		m_address(nullptr),
		m_useragent(nullptr),
		m_languages(nullptr) {}
	std::string m_sessionKey;
	char const* m_address;
	char const* m_useragent;
	char const* m_languages;
};

struct SessionDetectStorage {
	inline SessionDetectStorage() {}
	inline SessionDetectStorage(SessionDetectData const& sdd) :
		m_sessionKey(sdd.m_sessionKey),
		m_address(sdd.m_address ? sdd.m_address : ""),
		m_useragent(sdd.m_useragent ? sdd.m_useragent : ""),
		m_languages(sdd.m_languages ? sdd.m_languages : "") {}
	
	std::string m_sessionKey;
	std::string m_address;
	std::string m_useragent;
	std::string m_languages;
};

namespace impl {
	static char const g_empty[] = "";
	template <typename T, typename U> struct str_chr_cmp;
	
	template <>
	struct str_chr_cmp<char const*, char const*> {
		static bool is_equal(char const* a, char const* b) {
			return ((!a) && (!b))
				|| ((!a) && b && (!b[0]))
				|| ((!b) && a && (!a[0]))
				|| (a && b && (strcmp(a,b)==0));
		}
	};
	template <>
	struct str_chr_cmp<std::string, char const*> {
		static bool is_equal(std::string const& a, char const* b) {
			return b ? a==b : a.empty();
		}
	};
	template <>
	struct str_chr_cmp<char const*, std::string> {
		static bool is_equal(char const* b, std::string const& a) {
			return b ? a==b : a.empty();
		}
	};
	template <> struct str_chr_cmp<std::string, std::string> {
		static bool is_equal(std::string const& a, std::string const& b) {
			return a==b;
		}
	};
	
	template <typename A, typename B>
	bool is_equal(A const& a, B const& b) {
		return str_chr_cmp<A,B>::is_equal(a,b);
	}
}

template <typename T, typename U>
bool sessionMatches(T const& a, U const& b, bool skipSessionKey = false)
{
	return
		(skipSessionKey || impl::is_equal(a.m_sessionKey, b.m_sessionKey)) &&
		(g_settings.m_sessionTargetScore <= 0 || (
		(
			((impl::is_equal(a.m_address, b.m_address)) ? g_settings.m_sessionIpScore : 0) +
			((impl::is_equal(a.m_useragent, b.m_useragent)) ? g_settings.m_sessionUserAgentScore : 0) +
			((impl::is_equal(a.m_languages, b.m_languages)) ? g_settings.m_sessionLanguageScore : 0)
		) >= g_settings.m_sessionTargetScore));
}


class SessionManager;
class LuaSessionInterface;

class Session {
	friend class SessionManager;
	friend class LuaSessionInterface;
	
public:
	typedef std::chrono::system_clock expiration_clock;
	typedef std::map<std::string, Lua::ReturnValues> Realm;
	
private:
	rw_mutex m_mutex;
	std::map<std::string, Realm> m_realms;
	std::atomic<std::time_t> m_expiration;
	std::string m_sessionKey;
	SessionDetectStorage m_sds;

	struct RealmIterator {
		inline RealmIterator(
			std::map<std::string, Realm>::iterator beg,
			std::map<std::string, Realm>::iterator end
		) : b(beg), e(end) {}
		std::map<std::string, Realm>::iterator b;
		std::map<std::string, Realm>::iterator e;
		std::map<std::string, Realm>::iterator begin() const { return b; }
		std::map<std::string, Realm>::iterator end() const { return e; }
	};

	// DOES NOT LOCK!
	RealmIterator GetRealms(std::string const& realm, bool bCreate);
	
	// DOES NOT LOCK!
	void Touch();
	
public:
	Session(SessionManager*, std::string, SessionDetectData const&);
	
	bool IsValid();
	void Start();
	void Delete();
	bool HasRealm(std::string const& realm);
	void Clear(std::string const& realm);
	bool SetVar(std::string const& realm, std::string const& var, Lua::Variable* data);
	Lua::ReturnValues GetVar(std::string const& realm, std::string const& var);
	
	bool Matches(SessionDetectData*);
};


class LuaSessionInterface {
	SessionManager* m_manager;
	Session* m_realSession;
	SessionDetectData m_sdd;
	
	void CreateNewSessionTicket();
	void DeleteSessionTicket();
	
	void write();
	bool read() const;
public:
	LuaSessionInterface();
	void Init(SessionManager&, SessionDetectData const&);
	
	bool hasRealSession() const;
	void Start();
	void Delete();
	bool HasRealm(std::string const& realm);
	void Clear(std::string const& realm);
	bool SetVar(std::string const& realm, std::string const& var, Lua::Variable* data);
	Lua::ReturnValues GetVar(std::string const& realm, std::string const& var);
	
	bool getCookieString(std::string&) const;
};


class SessionManager {
	rw_mutex m_mutex;
	std::map<std::string, Session> m_sessions;
	
	// Create a new Session Key.
	static void CreateSessionKey(std::string&);
	
public:
	// Remove all the expired sessions. Currently unused and unimplemented.
	void CleanExpiredSessions();
	
	// Create a new empty session
	Session* CreateSession(SessionDetectData const& sdd);
	
	// Delete a session's data
	void DeleteSession(Session*);
	
	// Find an existing session
	Session* findSession(SessionDetectData const&);
};
extern SessionManager g_sessions;

#endif
