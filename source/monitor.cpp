#include "monitor.h"
#include <c++/monitor_factory.hpp>
#include <c++/libfswatch_exception.hpp>

// Not a fully featured path simplifier, but it's just a safety measure.
// Regular URLs will not be heavily impacted by this function.
std::string FileMonitor::simplify(std::string const& src)
{
	std::string dst;
	dst.reserve(src.size());
	
	std::string::size_type i = 0;
	std::string::size_type len = src.size();
	std::string dir;
	
	while(i < len)
	{
		while(i < len && (src[i] == '/' || src[i] == '\\'))
			++i;
		
		std::string::size_type begin = i;
		while(i < len && src[i] != '/' && src[i] != '\\')
			++i;
		
		dir.assign(src, begin, i-begin);
		
		if(dir == ".." || dir == ".")
			continue;
		
		dst = dst + "/" + dir;
	}
	return dst;
}

FileMonitor::FileMonitor()
	: m_monitor(nullptr)
{}

FileMonitor::~FileMonitor() {
	Close();
}

bool FileMonitor::iInitialized() const {
	return !!m_monitor;
}

bool FileMonitor::Initialized() {
	m_mx.lock_read();
	std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
	
	return iInitialized();
}

void fmon_event_callback(std::vector<fsw::event> const& events, void* fmon)
{
	if(!fmon)
		return;
	reinterpret_cast<FileMonitor*>(fmon)->HandleEvents(events);
}

bool FileMonitor::Init(char const* dir) {
	m_mx.lock();
	std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
	
	try {
		if(!m_monitor)
		{
			m_monitor = fsw::monitor_factory::create_monitor(fsw_monitor_type::inotify_monitor_type, std::vector<std::string>{std::string(dir)}, fmon_event_callback, this);
			if(!m_monitor)
			{
				LogError("Unable to create fswatch monitor!");
				return false;
			}
		}
		
		std::vector<fsw_event_type_filter> g_event_type_filters = {
			{Created},
			{Updated},
			{Renamed},
			{MovedFrom},
			{MovedTo}
		};
		
		m_monitor->set_allow_overflow(false);
		//m_monitor->set_latency(1);
		m_monitor->set_recursive(true);
		m_monitor->set_directory_only(false);
		m_monitor->set_event_type_filters(g_event_type_filters);
		m_monitor->set_follow_symlinks(true);
		m_monitor->set_watch_access(false);
	}
	catch(fsw::libfsw_exception& lex) {
		LogError("Failed to initialize libfswatch! Exception data:");
		LogError(lex.what());
		LogError(std::string("Error Code: ") + std::to_string(lex.error_code()));
		if(m_monitor)
		{
			delete m_monitor;
			m_monitor = nullptr;
		}
		return false;
	}
	catch(...) {
		LogError("Failed to initialize libfswatch!");
		if(m_monitor)
		{
			delete m_monitor;
			m_monitor = nullptr;
		}
		return false;
	}
	return true;
}

void FileMonitor::Close() {
	m_mx.lock_read();
	std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
	
	if(!m_monitor)
		return;
	
	if(m_monitor->is_running())
		m_monitor->stop();
	
	m_mx.chlock_w();
	delete m_monitor;
	m_monitor = nullptr;
}

std::int64_t FileMonitor::GetChangeId(std::string const& file) {
	m_mx.lock_read();
	std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
	auto it = m_revisions.emplace(file, FileRevisionData());
	FileRevisionData& rd = it.first->second;
	rd.NewRevision = false;
	return rd.RevisionNumber;
}

void FileMonitor::iTouchFile(std::string const& file, bool deleted) {
	auto revIt = m_revisions.find(file);
	if(revIt == m_revisions.end())
		return;
	FileRevisionData& rd = revIt->second;
	
	if(!rd.NewRevision) {
		rd.NewRevision = true;
		rd.RevisionNumber = (rd.RevisionNumber >= 0) ? (rd.RevisionNumber) : (-rd.RevisionNumber);
		++rd.RevisionNumber;
		if(rd.RevisionNumber < 0) // Overflow
			rd.RevisionNumber = 1;
		
		if(deleted)
		{
			rd.RevisionNumber = -rd.RevisionNumber;
			if(rd.RevisionNumber > 0) // Underflow
				rd.RevisionNumber = -1;
		}
	}
}

void FileMonitor::Run() {
	{
		m_mx.lock_read();
		std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
		if(!m_monitor)
			return;
	}
	
	
	try {
		m_monitor->start();
	} catch(...) {
		LogError("Exception thrown from libfswatch!");
		m_mx.chlock_w();
		std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
		try { delete m_monitor; } catch(...) {}
		m_monitor = nullptr;
	}
}

void FileMonitor::HandleEvents(std::vector<fsw::event> const& events) {
	m_mx.lock();
	std::lock_guard<rw_mutex> lg(m_mx, std::adopt_lock);
	for(auto it = events.begin(); it != events.end(); ++it)
	{
		bool isDeleted = false;
		bool isDir = false;
		
		std::vector<fsw_event_flag> flags = it->get_flags();
		for(auto xit = flags.begin(); xit != flags.end(); ++xit)
		{
			switch(*xit)
			{
			case MovedFrom:
			case Removed:
				isDeleted = true;
				break;
			case IsDir:
				isDir = true;
				break;
			default:
				break;
			}
		}
		
		if(!isDir)
			iTouchFile(it->get_path(), isDeleted);
	}
}

FileMonitor g_fmon;