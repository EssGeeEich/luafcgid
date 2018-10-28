function main()
	Send("<h1>Environment</h1><pre>\n")
	for n, v in pairs(Env) do
		Send(string.format("%s = %s\n", n, v))
	end
	Send("</pre>\n")

	if Env.REQUEST_METHOD == "GET" then
		s = {}  -- string sets are faster then calling Send() all the time
		params = lf.parse(Env.QUERY_STRING)
		table.insert(s, "<h1>GET Params</h1><pre>\n")
		for n, v in pairs(params) do
			table.insert(s, string.format("%s = %s\n", n, v))
		end
		table.insert(s, "</pre>\n")
		Send(table.concat(s))
	end

	if Env.REQUEST_METHOD == "POST" then
		Send("<h1>POST</h1>\n")
		Send(string.format("<textarea>%s</textarea>", Receive()))
	end
	
	Send("<h1>Info</h1><pre>\n")
	for k,v in pairs(Info) do
		Send(string.format("Info.%s = %s\n", k, v))
	end
	
	Send("</pre><h1>Debug</h1><pre>\n")
	for k,v in pairs(ThreadData()) do
		Send(string.format("%s = %s\n", k, v))
	end
	Send("</pre>")
	
	--[[
		These are all the exposed functions/variables:
		
		-- VARIABLES --
		Env -> FCGI Environment (table)
		Info -> State and Thread debug info
			Info.State = Id of Lua State that is "serving" us. Unique for each script.
			Info.Thread = Id of the thread that is "serving" us.
		Response -> Table of HTTP Status Codes
			Response[200] = "200 OK"
			Response[404] = "404 Not Found"
		
		-- FUNCTIONS --
		Header("X-PoweredBy", "luafcgid2")
			-> Function to add a response header
		
		Send("abc")
			-> Function to add something to the response itself
		
		Reset()
			-> Reset the headers and response buffers
		
		Log("TODO: Fix this")
			-> Send something to the error log
		
		Receive()
			-> Read submitted data from the client (eg POST data)
		
		RespStatus(Response[403])
			-> Change the response status
		
		RespContentType("text/plain")
			-> Change the response content-type
		
		ThreadData()
			-> Debug thread data information.
			Returns a table in which the keys are script names,
			 and the values are the number of loaded scripts.
	]]
end
