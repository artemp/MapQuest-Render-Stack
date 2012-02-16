#!/usr/bin/env python
from paste.script.util.logging_config import fileConfig
import os

BASEDIR = '/data/mapquest/storage_node_pylons'
INIFILE = os.path.join(BASEDIR, 'conf', 'production.ini')
TEMPDIR = '/data/mapquest/tmp'

# set the temporary file location to somewhere that isn't going to 
# interfere with anything else. WSGI spools incoming requests here,
# so it's necessary to put it somewhere where the files can grow to
# some size...
import tempfile
if not os.path.isdir(TEMPDIR):
    os.makedirs(TEMPDIR, mode=0777)
tempfile.tempdir = TEMPDIR

# Add the virtual Python environment site-packages directory to the path
import site
site.addsitedir(BASEDIR)

# Avoid ``[Errno 13] Permission denied: '/var/www/.python-eggs'`` messages
os.environ['PYTHON_EGG_CACHE'] = os.path.join(BASEDIR, 'egg-cache')

# load logging config
fileConfig( INIFILE )

# Load the Pylons application
from paste.deploy import loadapp
application = loadapp("config:%s" % INIFILE)

