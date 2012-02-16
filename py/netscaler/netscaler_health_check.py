import os, stat
import datetime

from mod_python import apache
def handler(req):
	curr_time = datetime.datetime.now ()
	hostname = os.environ ["HOSTNAME"]
        req.content_type = "text/html"

	log_timestamp_output = "<br>"
	log_dirname = os.path.join ("/data/mapquest/logs/render_stack", hostname, "worker")
	log_filenames = os.listdir (log_dirname)
	for log_filename in log_filenames:
		if os.path.splitext (log_filename)  [1] == ".log":
			path_and_filename = os.path.join (log_dirname, log_filename)
			last_active_time = datetime.datetime.fromtimestamp (os.stat (path_and_filename) [stat.ST_MTIME])
			inactivity_time = (curr_time - last_active_time).seconds
			if inactivity_time < 60:
				color = '#00FF00'
			elif inactivity_time < 300:
				color = 'FFFF00'
			else:
				color = '#FF0000'
			log_timestamp_output += "<font color='%s'>Log file:  %s  last updated at:  %s - inactive for %d seconds</font><br>" % (color, path_and_filename, str (last_active_time), inactivity_time)  

			log_timestamp_output += "<br>"
		
        # By convention, location of Netscaler healthcheck directory
        ns_dir = os.path.join ('/data/mapquest/ns/render_stack/', hostname)
        if os.path.exists(ns_dir) == False:
                os.makedirs(ns_dir)

        # Filename constants
        force_file     = 'force-traffic.txt'
        quiesce_file   = 'quiesce-traffic.txt'
        drain_file     = 'drain-traffic.txt'

        # Check for presence of force file

        abs_file = os.path.join(ns_dir, force_file)

        if (True == os.path.isfile(abs_file)):
                status = '200 OK'
                message = abs_file + ' exists. ' + status
		req.write (message)
		return apache.OK

        # Check for presence of drain file

        abs_file = os.path.join(ns_dir, drain_file)

        if (True == os.path.isfile(abs_file)):
                status = '500 NS Internal Server Error'
                message = abs_file + ' exists. ' + status
                req.write (message)
                return apache.OK

        # Check for presence of quiesce file

        abs_file = os.path.join(ns_dir, quiesce_file)

        if (True == os.path.isfile(abs_file)):
                status = '500 NS Internal Server Error'
                message = abs_file + ' exists. ' + status
                req.write (message)
                return apache.HTTP_BAD_REQUEST 

        # Default

        status = '200 OK'
        message = log_timestamp_output + ns_dir + ' is empty.  200 OK'
        req.write (message)
        return apache.OK
