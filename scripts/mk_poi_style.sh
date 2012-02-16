#!/usr/bin/env bash
if [ -z $1 ]; then
	DEST=./newstyle-github
else
	DEST=$1
fi

if [ -z $2 ]; then
	SRC=../style/clickable-style
else
	SRC=$2
fi

#pass filename, starttag, endtag, prints everything not between start and end tags inclusive of the tags
function strip 
{

	gawk -v start="$2" -v end="$3" '{
               #find the start tag
               if($0 ~ start)
               {
                  flag = 1
                  next
               }
               #stop at the end tag
               if($0 ~ end && flag == 1)
               {
                  flag = 0
                  next
               }
               #output any lines that we arent ignoring
               if(flag == 0)
               {
                  print $0
               } 
              }' < $1
}

#add the meta-writer plugin settings to the entities list
sed -i "s/<\!ENTITY % settings SYSTEM \"settings.xml.inc\">/<\!ENTITY % settings SYSTEM \"settings.xml.inc\">\n<\!ENTITY metawriter-settings SYSTEM \"metawriter-settings.xml.inc\">/g" $DEST/mapquest_inc/entities.xml.inc

#load the clickable layer with the rest of the us layers
sed -i "s/<\!ENTITY layer-base SYSTEM \"layer-base.xml.inc\">/<\!ENTITY layer-base SYSTEM \"layer-base.xml.inc\">\n<\!ENTITY layer-clickable-pois SYSTEM \"layer-clickable-pois.xml.inc\">/g" $DEST/mapquest_inc/layers-us.xml.inc

#remove train stations from the us layer symbols
#sed -n '1h;1!H;${;g;s@<!--train.*+?<\/Rule>@X@g;p;}' $DEST/mapquest_inc/layer-symbols-us.xml.inc
tmp=`strip $DEST/mapquest_inc/layer-symbols-us.xml.inc "--train" "</Rule>"`
echo "$tmp" > $DEST/mapquest_inc/layer-symbols-us.xml.inc

#remove museums from the queries
sed -i "/^.*\"tourism.*museum.*$/d" $DEST/mapquest_inc/layer-symbols-us.xml.inc

#use the meta-writer plugin in the us style
sed -i 's/settings;$/\0\n\&metawriter-settings;/g' $DEST/mapquest-us.xml

#insert the clickable layer in the us style
sed -i 's/^.*layer-amenity-symbols;.*$/\&layer-clickable-pois;\n\0/g' $DEST/mapquest-us.xml

#copy the clickable layer
cp -rp $SRC/mapquest_inc/layer-clickable-pois.xml.inc $DEST/mapquest_inc/layer-clickable-pois.xml.inc

#copy the meta-writer settings
cp -rp $SRC/mapquest_inc/metawriter-settings.xml.inc $DEST/mapquest_inc/metawriter-settings.xml.inc
