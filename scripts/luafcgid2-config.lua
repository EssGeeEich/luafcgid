--[[ configuration script for luafcgid2 ]]--

-- port or socket path to listen to
listen = "/var/tmp/luafcgid2.sock"

-- number of worker threads
threads = 4

-- starting number of Lua VM states per-script
states = 3

-- attempts to find an existing Lua VM state before creating a new one
retries = 3

-- max Lua VM states per-script
-- (whenever a request fails the retries-check, the new Lua VM state is added to the existing state,
--  if the max limit has not been exceeded)
maxstates = 5

-- starting buffer size for custom HTTP headers 
headersize = 64

-- starting buffer size for HTTP body 
bodysize = 1024

-- custom headers to add to all requests
headers = "X-Powered-By: luafcgid2\r\n"

-- default HTTP status
httpstatus = "200 OK"

-- default HTTP content type
contenttype = "text/html"

-- max POST size allowed
maxpost = 1024 * 1024

-- full or relative path to logfile
logfile = "/var/log/luafcgid2/luafcgid2.log"

-- load this file at the top of all scripts
-- please note that this file is only loaded once.
script = ""

-- entrypoint
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