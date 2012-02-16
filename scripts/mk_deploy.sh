#!/bin/bash

set -e

usage() {
    echo "Usage: `basename $0` [options]"
    echo "    -H <host> (repeat this for each host)"
    echo "    -b <num_brokers> (default: 4)"
    echo "    -c <coverage dir>"
    echo "    -h <num_handlers> (default: 4)"
    echo "    -n <max open FDs> (default: \`ulimit -n\` - 10)"
    echo "    -p <mapnik database password> (default: osm)"
    echo "    -s <stdbase installed location>"
    echo "    -u <mapnik database user> (default: osm)"
    echo "    -w <num_workers> (default: 4)"
    echo "    -S <style_alias> (default: newstyle-github)"
    echo "    -l <log file directory> (default: logs/)"
    echo "    -P <mongrel2 listen port> (default: 8002)"
    exit 1
}

section() {
    echo -ne "\E[32m\033[1m"
    echo -n " == $1 == "
    echo -e "\033[0m"
    tput sgr0
}

NO_ARGS=0
if [[ $# -eq "$NO_ARGS" ]]
then
    usage
fi

# setup default options
TARGET_DIR=$PWD
SOURCE_DIR=`dirname $0`/..
NUM_BROKERS=4
NUM_HANDLERS=4
NUM_WORKERS=4
HOSTS=""
DB_USER="osm"
DB_PASS="osm"
COVERAGE=""
let MAX_FD="`ulimit -n` - 20"
STYLE=newstyle-github
LOG_DIR="logs"
MONGREL2_PORT=8002

while getopts "b:h:H:s:u:p:w:c:n:S:l:P:" opt
do
    case $opt in
	b) NUM_BROKERS=$OPTARG;;
	h) NUM_HANDLERS=$OPTARG;;
	H) HOSTS="${HOSTS}${OPTARG} ";;
	w) NUM_WORKERS=$OPTARG;;
	s) STDBASE=$OPTARG;;
	u) DB_USER=$OPTARG;;
	p) DB_PASS=$OPTARG;;
	c) COVERAGE=$OPTARG;;
	n) MAX_FD=$OPTARG;;
	S) STYLE=$OPTARG;;
	l) LOG_DIR=$OPTARG;;
	P) MONGREL2_PORT=$OPTARG;;
	*) echo "Unknown option."; usage;;
    esac
done

# manage $PATH to include stdbase
if [[ -z "$STDBASE" ]]
then
    # default location if stdbase has been pre-built
    STDBASE="${SOURCE_DIR}/../../stdbase"
fi
PATH=$STDBASE/bin:$PATH
if [[ -z "$LD_LIBRARY_PATH" ]]
then
    LD_LIBRARY_PATH=$STDBASE/lib:$STDBASE/lib64
else
    LD_LIBRARY_PATH=$STDBASE/lib:$STDBASE/lib64:$LD_LIBRARY_PATH
fi
export PATH
export LD_LIBRARY_PATH

# if no hosts, assume localhost
if [[ -z "$HOSTS" ]]
then 
    # don't use the BCS hostname, it doesn't support -s, amazingly...
    HOSTS=`/bin/hostname -s`
fi

# go to target directory
mkdir -p $TARGET_DIR
pushd $TARGET_DIR

# make the directories that this needs
mkdir -p run tmp world_boundaries

# create or link the log directory, as ops want all log files
# in /data/mapquest/logs and this is the easiest way to do it.
if [ $LOG_DIR = "logs" ]
then
    mkdir -p logs
else
    ln -s $LOG_DIR logs
fi

# grab world boundaries things
section "Fetching world boundaries and other static files"
if [ -e "world_boundaries/.installed_ok" ]
then
    echo " >>> Already installed."
else
    pushd world_boundaries

    wget -O mercator_tiffs.tar http://developer.mapquest.com/content/static/geotiffs/mercator_tiffs.tar
    tar xf mercator_tiffs.tar
    mv geotiffs/*.tif .
    rm -rf geotiffs
    rm -f mercator_tiffs.tar
    
    wget -O world_boundaries-spherical.tgz http://tile.openstreetmap.org/world_boundaries-spherical.tgz
    tar zx -C .. -f world_boundaries-spherical.tgz
    rm -f world_boundaries-spherical.tgz
    
    wget -O processed_p.tar.bz2 http://tile.openstreetmap.org/processed_p.tar.bz2
    tar jxf processed_p.tar.bz2
    rm -f processed_p.tar.bz2
    
    wget -O shoreline_300.tar.bz2 http://tile.openstreetmap.org/shoreline_300.tar.bz2
    tar jxf shoreline_300.tar.bz2
    rm -f shoreline_300.tar.bz2
    
    # because these are so huge, we don't want to keep downloading them every time...
    touch ".installed_ok"
    popd
fi

# make profiles entries for each 
section "Making profiles"

# clean out any existing, old profiles
if [ -d "profiles" ]
then
    echo " >>> removing old copies of profiles"
    rm -rf profiles
fi
mkdir profiles
pushd profiles

# one profiles directory for each host
for host in $HOSTS
do
    mkdir $host
    pushd $host
    safe_host=$(echo $host | tr ".-" "__")

mkdir mongrel2
sed 's/^X//' << EOF > 'mongrel2/run'
#!/bin/sh
cd $TARGET_DIR
export PATH=$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH
m2sh start --db config.sqlite --name ${safe_host} >> logs/mongrel2_${host}_output_${MONGREL2_PORT}.log 2>&1 &
EOF

for ((i=0;i<$NUM_HANDLERS;++i))
do
    mkdir "tile_handler${i}"
    UUID=`uuidgen`
    sed 's/^X//' << EOF > "tile_handler${i}/run"
#!/bin/bash
set -e
cd $TARGET_DIR
export PATH=$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH
tile_handler -u ${UUID} >> logs/tile_handler_${host}_${i}.log 2>&1 &
echo \$! > profiles/${host}/tile_handler${i}/tile_handler${i}.pid
EOF
    echo -n "mongrel2 " > "tile_handler${i}/depends"
    for ((j=0;j<$NUM_BROKERS;++j))
    do
	echo -n "tile_broker${j} " >> "tile_handler${i}/depends"
    done
    echo >> "tile_handler${i}/depends"
done

for ((i=0;i<$NUM_BROKERS;++i))
do
    mkdir "tile_broker${i}"
    sed 's/^X//' << EOF > "tile_broker${i}/run"
#!/bin/bash
set -e
cd $TARGET_DIR
export PATH=$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH
tile_broker dqueue.conf broker_${safe_host}_${i} >> logs/tile_broker_${host}_${i}.log 2>&1 &
echo \$! > profiles/${host}/tile_broker${i}/tile_broker${i}.pid
EOF
done

for ((i=0;i<$NUM_WORKERS;++i))
do
    mkdir "worker${i}"
    sed 's/^X//' << EOF > "worker${i}/run"
#!/bin/bash
set -e
cd $TARGET_DIR
export PATH=$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH
# sleep a second to wait for the broker - otherwise the worker
# binds (!?) the port... yeah, i know it's not supposed to.
sleep 2
python -u ${SOURCE_DIR}/py/worker.py worker.conf dqueue.conf >> logs/worker_${host}_${i}.log 2>&1 &
echo \$! > profiles/${host}/worker${i}/worker${i}.pid
EOF
    echo -n > "worker${i}/depends"
    for ((j=0;j<$NUM_BROKERS;++j))
    do
	echo -n "tile_broker${j} " >> "worker${i}/depends"
    done
    echo >> "worker${i}/depends"
done

for i in *
do
    chmod +x $i/run
    touch $i/restart
    touch $i/depends
    echo "${TARGET_DIR}/profiles/${host}/${i}/${i}.pid" > $i/pid_file
done

# link procer log files into the log directory
touch ../../logs/procer_${host}_run.log
touch ../../logs/procer_${host}_error.log
ln -s ../../logs/procer_${host}_run.log run.log
ln -s ../../logs/procer_${host}_error.log error.log

    popd # leaving host-specific dir
done # leaving hosts loop

popd # leaving profiles dir

# write style files
section "Writing style files"

# first make the config
sed 's/^X//' <<EOF > mk_config.conf
[default]
epsg = 900913
estimate_extent = false
extent = -20037508,-19929239,20037508,19929239
prefix = planet

dbname = gis
host = localhost
port = 5432
user = ${DB_USER}
password = ${DB_PASS}

world_boundaries = ${TARGET_DIR}/world_boundaries

;; the mapquest and hybrid styles have different symbols directories,
;; so fill out this section to have them point to the proper places.
[mapquest_inc]
symbols = ${TARGET_DIR}/${STYLE}/mapquest_symbols
[hybrid_inc]
symbols = ${TARGET_DIR}/${STYLE}/hybrid_symbols
EOF

# now grab the github style and the mask
python ${SOURCE_DIR}/py/mk_config.py mk_config.conf ${SOURCE_DIR}/style/${STYLE}/ ${STYLE}
python ${SOURCE_DIR}/py/mk_config.py mk_config.conf ${SOURCE_DIR}/style/mask2/ mask2

# and grab the WKT files for the mask, too
for poly in us uk
do
    cp ${SOURCE_DIR}/style/mask2/${poly}.wkt mask2/
done

# grab the SVG and other icon files too
mkdir -p ${STYLE}/mapquest_symbols
mkdir -p ${STYLE}/hybrid_symbols
cp ${SOURCE_DIR}/style/${STYLE}/mapquest_symbols/* ${STYLE}/mapquest_symbols/
cp ${SOURCE_DIR}/style/${STYLE}/hybrid_symbols/* ${STYLE}/hybrid_symbols/

# write the config files
section "Writing config files"
sed 's/^X//' <<EOF > mongrel2.conf
render_handler = Handler(send_spec='ipc:///tmp/mongrel_send',
X                       send_ident='MONGREL2',
X                       recv_spec='ipc:///tmp/mongrel_recv', recv_ident='')
netscaler_dir = Dir(base='netscaler/', index_file='nstest.html',
X                 default_ctype='text/html')
crossdomain_dir = Dir(base='crossdomain/', index_file='crossdomain.xml',
X                 default_ctype='application/xml')
EOF

servers=""
for host in $HOSTS
do
    safe_host=$(echo $host | tr ".-" "__")
    servers="${servers},tile_server_${safe_host}"
    sed 's/^X//' <<EOF >> mongrel2.conf
tile_server_${safe_host} = Server(
X    uuid="$(uuidgen)",
X    access_log="/logs/mongrel2_${host}_access_${MONGREL2_PORT}.log",
X    error_log="/logs/mongrel2_${host}_error_${MONGREL2_PORT}.log",
X    chroot="${TARGET_DIR}",
X    default_host="(.+)",
X    name="${safe_host}",
X    pid_file="/profiles/${host}/mongrel2/mongrel2.pid",
X    port=${MONGREL2_PORT},
X    hosts = [
X        Host(name="(.+)", routes={
X            '/tiles/1.0.0' : render_handler,
X            '/_ns_' : netscaler_dir
X            '/crossdomain.xml' : crossdomain_dir
X        })
X    ]
)
EOF
done

servers=$(echo $servers | sed "s/^,//")
sed 's/^X//' <<EOF >> mongrel2.conf
settings = {"zeromq.threads": 1, "superpoll.max_fd": ${MAX_FD}}
servers = [${servers}]
EOF

# now make the SQLite database
m2sh load --db config.sqlite --config mongrel2.conf

# tile handler config is same for all hosts
sed 's/^X//' <<EOF > tile_handler.conf
[mongrel2]
in_endpoint  = ipc:///tmp/mongrel_send 
out_endpoint = ipc:///tmp/mongrel_recv
max_age = 600

;; tile storage
[tiles]
type = disk
tile_dir = ${TARGET_DIR}/tiles

;; make 'map' a synonym of 'osm'
[rewrite]
map = osm

;; available formats for fast 404ing
[formats]
osm = png, jpeg
hyb = png
EOF

# worker config
sed 's/^X//' <<EOF > worker.conf
[worker]
styles = osm, hyb

[storage]
type = disk
tile_dir = ${TARGET_DIR}/tiles

[coverages]
osm = coverage/map/
hyb = coverage/map/

[formats]
osm = png256, jpeg
hyb = png

[osm]
default_style = ${STYLE}/mapquest-eu.xml
mask_style = mask2/mask.xml
mask2/us.wkt = ${STYLE}/mapquest-us.xml
mask2/uk.wkt = ${STYLE}/mapquest-uk.xml
system = mapnik
type = osm

[hyb]
default_style = ${STYLE}/mapquest-hybrideu.xml
mask_style = mask2/mask.xml
mask2/us.wkt = ${STYLE}/mapquest-hybridus.xml
mask2/uk.wkt = ${STYLE}/mapquest-hybriduk.xml 
system = mapnik
type = hyb

[png]
optimize = true

[jpeg]
quality = 95
optimize = true
progressive = true

[png256]
pil_name = png
optimize = true
palette = true
EOF

# distributed queue config
broker_names=""
for host in $HOSTS
do
    safe_host=$(echo $host | tr ".-" "__")
    for ((i=0;i<$NUM_BROKERS;++i))
    do
	broker_names="${broker_names},broker_${safe_host}_${i}"
    done
done
broker_names=$(echo $broker_names | sed "s/^,//")

sed 's/^X//' <<EOF > dqueue.conf
[backend]
type = zmq

;; broker endpoint
[zmq]
broker_names = ${broker_names}
heartbeat_time = 5

[worker]
poll_timeout = 5

EOF

for host in $HOSTS
do
    base_port=44444
    safe_host=$(echo $host | tr ".-" "__")
    for ((i=0;i<$NUM_BROKERS;++i))
    do
	ip=$(host $host | sed "s/.*address //")
	broker_name="broker_${safe_host}_${i}"
	let "in_req_port = $base_port"
	let "in_sub_port = $base_port + 1"
	let "out_req_port = $base_port + 2"
	let "out_sub_port = $base_port + 3"
	let "monitor_port = $base_port + 4"
sed 's/^X//' <<EOF >> dqueue.conf
[${broker_name}]
in_req = tcp://${ip}:${in_req_port}
in_sub = tcp://${ip}:${in_sub_port}
out_req = tcp://${ip}:${out_req_port}
out_sub = tcp://${ip}:${out_sub_port}
monitor = tcp://${ip}:${monitor_port}
in_identity = ${broker_name}_in
out_identity = ${broker_name}_out

EOF
        let "base_port += 10"
    done
done

# make an init script
sed 's/^X//' <<EOF > render_stack
#!/bin/bash
#
# render_stack The MQ Map Rendering System
#

. /etc/init.d/functions

export PATH=$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH

host=\$(/bin/hostname -s)
prog=procer

start() {
X  echo -n \$"Starting render stack: "
X  if [ -e /var/lock/subsys/\$prog ]; then
X    failure \$"Cannot start render stack - already running."
X    echo
X    return 1
X  fi
X  ${STDBASE}/bin/procer ${TARGET_DIR}/profiles/\$host ${TARGET_DIR}/profiles/\$host/procer.pid
X  if [ \$(id -u) -eq 0 ]; then
X    touch /var/lock/subsys/\$prog
X  fi
X  success
X  echo
X  return 0
X}
X
Xstop() {
X  echo -n \$"Stopping render stack: "
X  m2sh stop -every -murder >/dev/null 2>&1
X  sleep 2
X  pid_files=\$(find ${TARGET_DIR}/profiles/\$host -name "*.pid")
X  for i in \$pid_files
X  do
X    kill -15 \$(cat \$i) >/dev/null 2>&1
X  done
X  sleep 3
X  for i in \$pid_files
X  do
X    kill -9 \$(cat \$i) >/dev/null 2>&1
X  done
X  if [ \$(id -u) -eq 0 ]; then
X    rm -f /var/lock/subsys/\$prog
X  fi
X  success
X  echo
X  return 0
X}
X
Xcase "\$1" in
X  start)
X    start;;
X  stop)
X    stop;;
X  *)
X    echo \$"Usage: $0 {start|stop}"
X    exit 1
Xesac
EOF

chmod +x render_stack

# serving this file tells the netscaler we're up & running OK
mkdir -p netscaler
sed "s/^X//" <<EOF > netscaler/nstest.html
<html>
Netscaler 200 OK
</html>
EOF

# serve this file up for flash
mkdir -p crossdomain
sed "s/^X//" <<EOF > crossdomain/crossdomain.xml
X<cross-domain-policy>
X  <site-control permitted-cross-domain-policies="all"/>
X  <allow-access-from domain="*" secure="true"/>
X</cross-domain-policy>
EOF

if [[ -n "$COVERAGE" ]]
then
    # set up coverage information
    section "Setting up coverage"
    rm -rf coverage
    mkdir coverage
    pushd coverage
    cp -r $COVERAGE/* .
    for style in map hyb sat ter
    do
	pushd $style
	ln -s ../osmCopyright.cfg .
	ln -s ../navteqCopyright.cfg .
	ln -s ../icubedCopyright.cfg .
	ln -s ../intermapCopyright.cfg .
	ln -s ../andCopyright.cfg .
	ln -s ../mapquestCopyright.cfg .
	popd
    done
    popd
fi

# end
section "All done"
popd
