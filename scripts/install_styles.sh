#!/bin/bash

# first argument is the false prefix for package building
PKG_DIR=$1
# second argument is the "true" prefix for shared stuff
SHARE_DIR=$2
# third argument is style name
STYLE=$3
# fourth (optional) argument is directly providing the mk_config.conf file
# instead of creating it on the fly here
CONFIG=$4
# fifth (optional) argument is the target style name, if different
# from the style name above
TARGET_STYLE=$5

if [ "x$PKG_DIR" = "x" ]
then
    echo "Error - package directory not specified."
    exit 1
fi
if [ "x$SHARE_DIR" = "x" ]
then
    echo "Error - share directory not specified."
    exit 1
fi
if [ "x$STYLE" = "x" ]
then
    echo "Error - style name not specified."
    exit 1
fi
if [ "x$TARGET_STYLE" = "x" ]
then
    TARGET_STYLE=$STYLE
fi

# first make the config
if [ -z $CONFIG ]; then

sed 's/^X//' <<EOF > mk_config.conf
[default]
epsg = 900913
estimate_extent = false
extent = -20037508,-19929239,20037508,19929239
prefix = planet

dbname = gis
host = localhost
port = 5432
user = mqmgr
password = 

world_boundaries = ${SHARE_DIR}/world_boundaries

;; the mapquest and hybrid styles have different symbols directories,
;; so fill out this section to have them point to the proper places.
[mapquest_inc]
symbols = ${SHARE_DIR}/style/${TARGET_STYLE}/mapquest_symbols
[hybrid_inc]
symbols = ${SHARE_DIR}/style/${TARGET_STYLE}/hybrid_symbols
EOF

else
	sed -e "s#\${SHARE_DIR}#${SHARE_DIR}#g" -e "s#\${TARGET_STYLE}#${TARGET_STYLE}#g" ${CONFIG} > mk_config.conf;
fi

# now grab the style
python py/mk_config.py mk_config.conf style/${STYLE}/ ${PKG_DIR}${SHARE_DIR}/style/${TARGET_STYLE}

# and grab any WKT files (for the mask)
for poly in style/${STYLE}/*.wkt
do
	if [ -f $poly ]; then
		cp $poly ${PKG_DIR}${SHARE_DIR}/style/${TARGET_STYLE}/
	fi
done

# grab the SVG and other icon files too
if [ -d style/${STYLE}/mapquest_symbols ]
then
    mkdir -p ${PKG_DIR}${SHARE_DIR}/style/${TARGET_STYLE}/mapquest_symbols/
    cp style/${STYLE}/mapquest_symbols/* ${PKG_DIR}${SHARE_DIR}/style/${TARGET_STYLE}/mapquest_symbols/
fi

if [ -d style/${STYLE}/hybrid_symbols ]
then
    mkdir -p ${PKG_DIR}${SHARE_DIR}/style/${TARGET_STYLE}/hybrid_symbols/
    cp style/${STYLE}/hybrid_symbols/* ${PKG_DIR}${SHARE_DIR}/style/${TARGET_STYLE}/hybrid_symbols/
fi

# clean up
rm mk_config.conf
