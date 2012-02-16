#!/bin/bash 

BASE_DIR="/data/mapquest"
MQ_DIR="/data/mapquest"
STDBASE="$MQ_DIR/stdbase"
RENDER_STACK="$MQ_DIR/render_stack"

# more config parameters
BBOX="-180,-85,180,85"
STYLE="$RENDER_STACK/share/style/osm2pgsql/default.style"
DB="gis"
PLANET_PREFIX="planet"

# path setup
export LD_LIBRARY_PATH="$STDBASE/lib"
export PATH="$STDBASE/bin:$RENDER_STACK/bin:$PATH:/opt/bcs/bin"

# programs we're going to use
OSM2PGSQL="$STDBASE/bin/osm2pgsql"
EXPIRE_TILES="$RENDER_STACK/bin/expire_tiles"
OSMOSIS="$MQ_DIR/osmosis-0.39/bin/osmosis"

# check they're all present
if [ ! -e $OSM2PGSQL ]; then
  echo "osm2pgsql ($OSM2PGSQL) not installed, but is required."
  exit 1
fi
if [ ! -e $EXPIRE_TILES ]; then
  echo "expire_tiles ($EXPIRE_TILES) from render_stack not installed, but is required."
  exit 1
fi
if [ ! -e $OSMOSIS ]; then
  echo "osmosis ($OSMOSIS) not installed, but is required."
  exit 1
fi
if [ ! -e $STYLE ]; then
  echo "osm2pgsql style file ($STYLE) could not be found, but is required."
  exit 1
fi

# directories used by the update process
WORKDIR_OSM="$BASE_DIR/osmosis_work_dir"
CHANGESET_DIR="$WORKDIR_OSM/minutely"
EXPIRED_DIR="$WORKDIR_OSM/dirty"

# make the directories
mkdir -p $WORKDIR_OSM $CHANGESET_DIR $EXPIRED_DIR

# expire (and re-render) tiles from z11-z15 and only expire those from
# z16-z18.
expire_tiles() {
  $EXPIRE_TILES --rerender-with $RENDER_STACK_CONF/dqueue.conf \
     --worker-config $RENDER_STACK_CONF/worker.conf \
     -d "/" --zxy -e 11 -E 18 -r 11 -R 15 $EXPIRED_DIR/dirty-current;
}

osmosis_fetch_changeset() {
  if [ ! -e $WORKDIR_OSM/state.txt ]; then
    echo "Osmosis state file not found - has the state been correctly initialised?"
    exit 1
  fi
  STATE_TIMESTAMP=`grep '^timestamp=' $WORKDIR_OSM/state.txt | tail -n1 | cut -c 11-`
  CURRENT_TIMESTAMP=`date -u "+%Y-%m-%d_%H:%M:%S"`
  CHANGESET_FILE=$CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz

  echo "$CURRENT_TIMESTAMP:Downloading changeset $STATE_TIMESTAMP"
  cp $WORKDIR_OSM/state.txt $CHANGESET_DIR/state-$STATE_TIMESTAMP
  $OSMOSIS --read-replication-interval workingDirectory=$WORKDIR_OSM \
    --simplify-change --write-xml-change $CHANGESET_FILE
}

osmosis_cleanup() {
  rm -f $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz
  rm -f $CHANGESET_DIR/state-$STATE_TIMESTAMP
  rm -f $EXPIRED_DIR/dirty-current;
}

update() {
  osmosis_fetch_changeset

  $OSM2PGSQL --append --prefix $PLANET_PREFIX -S$STYLE -b$BBOX -d$DB -s \
    -C 8192 $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz -e15-15 -x \
    -o $EXPIRED_DIR/dirty-current $CHANGESET_FILE

  # exit if osm2pgsql fails
  if [ $? -ne 0 ]
  then
    echo "osm2pgsql: failed to apply $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz"
    cp $CHANGESET_DIR/state-$STATE_TIMESTAMP $WORKDIR_OSM/state.txt
  else
    echo "osm2pgsql: applied $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz"
    expire_tiles
    echo "Done"
  fi

  osmosis_cleanup
}

catchup() {
  osmosis_fetch_changeset

  $OSM2PGSQL --append --prefix $PLANET_PREFIX -S$STYLE -b$BBOX -d$DB -s \
    -C 8192 $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz -x $CHANGESET_FILE

  # exit if osm2pgsql fails
  if [ $? -ne 0 ]
  then
    echo "osm2pgsql: failed to apply $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz"
    cp $CHANGESET_DIR/state-$STATE_TIMESTAMP $WORKDIR_OSM/state.txt
  else
    echo "osm2pgsql: applied $CHANGESET_DIR/changeset-$STATE_TIMESTAMP.ocs.gz"
  fi

  #osmosis_cleanup
}  

initialise() {
  if [ -e $WORKDIR_OSM/state.txt ]; then
    echo "Osmosis state file found - has this already been initialised?"
    exit 1
  fi
  PLANET=$BASE_DIR/planet/planet-latest.osm.bz2
  if [ ! -e $PLANET ]; then
    echo "Planet file not found - cannot initialise without this."
    exit 1
  fi
  $OSMOSIS --read-replication-interval-init workingDirectory=$WORKDIR_OSM
  PLANET_TIME=`bzcat $PLANET | head -n 3 | grep "osm.*timestamp=" | sed "s/.*timestamp=\"//;s/\".*//"`
  wget "http://toolserver.org/~mazder/replicate-sequences/?$PLANET_TIME" -O $WORKDIR_OSM/state.txt;
}

# make sure only one is running at any time...
LOCK_FILE="$BASE_DIR/locks/minutely_mapnik.lock"
mkdir -p "$BASE_DIR/locks"

(set -C; : > $LOCK_FILE) 2> /dev/null

if [ $? != "0" ]; then
   echo "Lock File exists - exiting"
   exit 1
fi

trap 'rm $LOCK_FILE' EXIT 1 2 3 6

case "$1" in
  update)
   update;;

  initialise)
    initialise;;

  catchup)
    catchup;;

  *)
    echo "Usage: $0 {update|initialise|catchup}"
    echo 
    echo "  update:     Update the database using Osmosis and minutely diffs."
    echo
    echo "  initialise: Set up osmosis replication - relies on the location of the"
    echo "              planet file from the import step."
    echo
    echo "  catchup:    Catch up the database. The same as update, except it does"
    echo "              not run the tile expiry tasks."
    exit 1;;
esac

