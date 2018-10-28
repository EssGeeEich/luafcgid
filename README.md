# Summary

luafcgid2 is a multithreaded FastCGI server that runs under BSD/Linux.
It manages a number of independent, persistent Lua states, that are then loaded
with Lua scripts from the file system. These scripts are loaded/initialized
on demand, and held in memory for as long as possible. The Lua scripts are also
allowed to interface with the FastCGI libraries: thus providing an extremely
fast, streamlined and lightweight platform from which to develop web-centric
apps in Lua.

# License

See the LICENSE file included with the source code

# Testing

All development testing is done with a Raspberry Pi Zero W.

## Software:

+ Raspbian Lite - last stable release
+ nginx web server - last stable release
+ Lua 5.3 - last stable release

# Prerequisites

+ Lua 5.3
+ libfcgi 2.4
+ libpthread

On a Raspberry Pi, simply run the following:

    # sudo apt-get -y install libfcgi-dev liblua5.3-dev git make build-essential
    # git clone git@github.com:EssGeeEich/luafcgid2.git
    # cd luafcgid2
    # make
    # sudo make install install-daemon

You may need to tinker with the Makefile if you aren't using a Raspberry Pi.

## Webserver (nginx):

Add the following lines to your /etc/nginx/sites-available/your-site-here in the server{} section:

	location ~ \.lua$ {
		fastcgi_pass   unix:/var/tmp/luafcgid2.sock;
		fastcgi_param  SCRIPT_FILENAME  $document_root$fastcgi_script_name;
		include        fastcgi_params;
	}

**NOTE:** _make sure your root directive is set correctly_
   
# Design

luafcgid2 spawns and manages a number of worker threads that each contain an 
isolated blocking accept loop. The FastCGI libraries provide a connect queue 
for each worker thread so that transient load spikes can be handled with a 
minimum of fuss.

	                  +---------------+                                
	              +-->| worker thread |--+            +-------------+
	+----------+  |   +---------------+  |            |   scripts   |
	| luafcgid |--+                      +-- mutex -->| loaded into |
	+----------+  |   +---------------+  |     |      |  Lua states |
	     |        +-->| worker thread |--+     |      +-------------+
	     |            +---------------+        |                
	     |                                     |             
	     +------------- housekeeping ----------+             
			
Lua is then introduced into the picture by created a shared Lua state for each 
Lua script that is requested. A script will be loaded multiple times.
All scripts - including duplicates (clones) - are completely isolated from each other.
After a state is initialized and loaded  with a script, it is kept in memory forever.
Each Lua VM is run within a worker thread as needed.
The use of on-demand clones allows for multiple workers to run the same popular script.
There is a configurable limit to the total number of Lua states that luafcgid will maintain.
When this limit is reached, a new state gets generated at runtime.
