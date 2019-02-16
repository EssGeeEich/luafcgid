#include "monitor.h"
#include <picosha2.h>

// Not a fully featured path simplifier, but it's just a safety measure.
// Regular URLs will not be heavily impacted by this function.
SimplifiedPath FileMonitor::simplify(std::string const& src)
{
	SimplifiedPath rval;
	std::string& dst = rval.m_path;
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
	return rval;
}

FileChangeData FileMonitor::getFileStatus(std::ifstream& f)
{
	FileChangeData fcd;
	
	if(g_settings.m_useFileChecksum)
	{
		// std::ifstream f(path.m_path, std::ios_base::binary);
		std::vector<std::uint8_t> s(picosha2::k_digest_size);
		picosha2::hash256(f, s.begin(), s.end());
		if(f)
		{
			std::swap(fcd.m_hash, s);
			fcd.m_filesize = f.tellg();
			fcd.m_exists = true;
		}
	}
	else
	{
		//std::ifstream f(path.m_path, std::ios_base::binary | std::ios_base::ate);
		f.seekg(0, std::ios_base::end);
		if(f)
		{
			fcd.m_filesize = f.tellg();
			fcd.m_exists = true;
		}
	}
	
	if(f)
	{
		f.seekg(0, std::ios_base::beg);
		f.clear();
	}
	fcd.m_captureTime = FileChangeData::clock_t::now();
	return fcd;
}

std::unique_ptr<std::ifstream> FileMonitor::getFileForLoading(SimplifiedPath const& path,
	FileChangeData& storage, bool forceReload)
{
	std::unique_ptr<std::ifstream> file;
	FileChangeData compare;
	if(forceReload)
	{
		file.reset(new std::ifstream(path.m_path, std::ios_base::binary));
		compare = FileMonitor::getFileStatus(*file);
		if(!*file)
			return std::unique_ptr<std::ifstream>();
	}
	else
	{
		if(std::chrono::duration_cast<std::chrono::milliseconds>(FileChangeData::clock_t::now() - storage.m_captureTime).count()
			<= g_settings.m_fileInfoTime)
			return std::unique_ptr<std::ifstream>();
		
		file.reset(new std::ifstream(path.m_path, std::ios_base::binary));
		compare = FileMonitor::getFileStatus(*file);
		if( (!*file)
			|| (compare == storage))
			return std::unique_ptr<std::ifstream>();
	}
	storage = compare;
	return file;
}
