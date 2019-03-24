#ifndef MONITOR_H_INCLUDED
#define MONITOR_H_INCLUDED
#include "settings.h"
#include <fstream>
#include <memory>
#include <atomic>
#include <map>

class SimplifiedPath {
	friend class FileMonitor;
	std::string m_path;
	std::string m_dir;
	std::string m_root;
public:
	// Const only
	inline std::string const& get() const {
		return m_path;
	}
	inline std::string const& dir() const {
		return m_dir;
	}
	inline std::string const& root() const {
		return m_root;
	}
};

struct FileChangeData {
	typedef std::chrono::steady_clock clock_t;
	inline FileChangeData()
		: m_exists(false), m_filesize(0) {}

	inline bool operator == (FileChangeData const& o) const {
		return
			m_exists == o.m_exists &&
			m_filesize == o.m_filesize &&
			m_hash == o.m_hash;
	}
	inline bool operator != (FileChangeData const& o) const {
		return !(*this == o);
	}

	bool m_exists;
	std::vector<std::uint8_t> m_hash;
	std::size_t m_filesize;

	clock_t::time_point m_captureTime;
};

class FileMonitor {
	static FileChangeData getFileStatus(std::ifstream&);
	FileMonitor() =delete;
public:
	static SimplifiedPath simplify(std::string const&, std::string const&);

	static std::unique_ptr<std::ifstream>
		getFileForLoading(
			SimplifiedPath const&,
			FileChangeData&,
			bool =false);
};

#endif
