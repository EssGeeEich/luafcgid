#include "settings.h"
#include "rw_mutex.h"
#include <map>
#include <atomic>
#include <c++/monitor.hpp>
#include <c++/event.hpp>

struct FileRevisionData {
	inline FileRevisionData() : RevisionNumber(0), NewRevision(false) {}
	
	std::int64_t RevisionNumber;
	bool NewRevision;
};

class FileMonitor {
	fsw::monitor* m_monitor;
	
	rw_mutex m_mx;
	std::string m_prebuffer;
	std::map<std::string, FileRevisionData> m_revisions;
	
	friend void fmon_event_callback(std::vector<fsw::event> const&, void*);
	FileMonitor& operator=(FileMonitor const&) =delete;
	FileMonitor(FileMonitor const&) =delete;
	
	bool iInitialized() const;
	void iTouchFile(std::string const& file, bool deleted);
	void HandleEvents(std::vector<fsw::event> const&);
public:
	FileMonitor();
	~FileMonitor();
	void Close();
	bool Initialized();
	bool Init(char const* dir);
	
	static std::string simplify(std::string const&);
	
	void Run();
	
	// simplify'ed path required
	// Returns a negative number if the file is missing.
	// Returns a number 1-n for every different revision of the file
	std::int64_t GetChangeId(std::string const&);
};

extern FileMonitor g_fmon;