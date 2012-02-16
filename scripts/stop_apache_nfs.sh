#!/bin/bash

export LD_LIBRARY_PATH=/data/mapquest/render_stack/runtime/lib:/data/mapquest/render_stack/runtime/lib64:/usr/lib:/lib:/data/mapquest/render_stack/runtime/apache2.2/lib
export PATH=/data/mapquest/render_stack/runtime/bin:/usr/bin:/bin:/usr/sbin:/sbin:/data/mapquest/render_stack/runtime/apache2.2/bin

which httpd
httpd -k stop -f /data/mapquest/render_stack/hosts/$(hostname)/apache/conf/httpd-nfs.conf 

