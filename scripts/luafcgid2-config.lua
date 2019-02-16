--[[ Configuration script for luafcgid2 ]]--

-- Amount of worker threads
WorkerThreads = 4

-- Number of default Lua states initially loaded
LuaStates = 1

-- Max number of Lua states (will be loaded as needed, but will never unload)
LuaMaxStates = 6

-- Max number of times to search for a free Lua state before creating a new ad-hoc one
LuaMaxSearchRetries = 3

-- Starting buffer size for custom HTTP headers
HeadersSize = 128

-- Starting buffer size for HTTP body
BodySize = 2048

-- Starting buffer is split in [BodySectors] number of [BodySize] byte chunks
-- Thanks to C++11, this method can vastly improve performance
BodySectors = 4

-- Time (in ms) that we consider a file to be unchanged
-- It is suggested to increase this value as your server needs to manage more users and is under heavier load
-- Having more threads will kind of ignore this variable.
MinFileInfoTime = 5000

-- Calculate a file's checksum to see if it changed
-- It is only calculated at most every [MinFileInfoTime] ms
UseFileChecksum = true

-- Custom HTTP headers. Will be added to every request.
DefaultHeaders = "X-Powered-By: luafcgid2\r\n"

-- Default HTTP status.
DefaultHttpStatus = "200 OK"

-- Default Content-Type
DefaultContentType = "text/html"

-- Max POST upload size allowed
MaxPostSize = 1048576 -- 1MB

-- Log File Path
LogFilePath = "/var/log/luafcgid2/luafcgid2.log"

-- Port or Socket path to listen to
listen = "/var/tmp/luafcgid2.sock"

-- Load this *file* at the top of all scripts
-- Please note that this file is only loaded once.
StartupScript = ""

-- Entrypoint
entrypoint = "main"

-- Config table, can be accessed from the scripts.
Config = {
	entrypoint = entrypoint,
	string_example = "Test config string",
	table_example = {
		"abc","def","ghi",
		number=42
	}
}