# MapQuest Render Stack

This is the software stack used at [MapQuest](http://www.mapquest.com)
to render map tiles. It was built to support distributed, asynchronous
map tile rendering and include multiple rendering styles and renderers
flexibly.

Note that this software is a first release and the build system was
recently converted to GNU Autotools, so it is quite likely that there
are still issues remaining. If you find build or other errors, please
report them on the [github issues
page](https://github.com/MapQuest/MapQuest-Render-Stack/issues).

## Installation

First, you'll need the prerequisites:

* GNU Autoconf >= 2.68
* GNU Automake >= 1.11.1
* ZeroMQ >= 2.1.10 < 3.0
* Google Protocol Buffers >= 2.4.0
* Mongrel2 >= 1.7.5
* libGD >= 2.0.35
* Mapnik >= 2.0
* libcurl >= 7.19.5
* libmemcached >= 0.49
* python >= 2.6
* boost >= 1.45
* libbz2 
* libpq

You should be able to run the standard autotools generate, configure,
build cycle:

 ./autogen.sh
 ./configure
 make && make install

## Configuration

Please see the examples/ directory for some example configuration
files. Note that configuration files will vary from location to
location and what works for us may not work for you.

Mongrel2 requires some special setup to load its configuration file
into a SQLite database before running, please see the Mongrel2
documentation for details.

## Running

The map stack expects an existing working PostGIS + Mapnik2 setup. If
this is already present, you have configured the worker to point
to this style file and the handler & worker storage configurations,
you should be able to run:

 ./tile_handler -c tile_handler.conf -C dqueue.conf &
 ./tile_broker dqueue.conf "broker_localhost" &
 python py/worker.py worker.conf dqueue.conf &

Mongrel2 should now be serving tiles on the port that you set up in
its configuration file.  
