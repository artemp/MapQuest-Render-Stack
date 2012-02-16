from storage_node_pylons.lib.base import BaseController
from pylons.controllers.util import Response
from pylons import config
from pylons.templating import render_mako as render
from storage_node_pylons.lib.mqStats import mqStats
import json
import logging

log = logging.getLogger(__name__)

class StatsController(BaseController):
    """ Controller for generating statistics outputs.
    """

    def document_html(self):
        """Render the statistics into an HTML document suitable
           for easy browsing and eyeball-monitoring.
        """
        stats = self.get_stats()

        # transform stats into a more useful form for the html template
        vars = dict()
        for method, arry in stats.items():
            for time in arry:
                # is it a stat
                if isinstance(time, dict):
                    for x in ['n', 'avg', 'dev']:
                        k = "%s_%s_%s" % (method, time['time'], x)
                        vars[str(k)] = str(time[x])
                # is it a status
                else:
                    vars[str("%s_status" % (method))] = time

        return render('/stats.html', extra_vars=vars)

    def document_json(self):
        """Render the statistics to a JSON document, ideal for inclusion into
           other systems that then don't have to parse the HTML.
        """
        stats = self.get_stats()

        page = json.dumps(stats)
        return Response(body=page, content_type='application/json;charset=UTF-8')

    def get_stats(self):
        """Gets the statistics (gets, puts) averages.
        """
        stats = config['pylons.app_globals'].statsDB()
        data = stats.get_stats()
        data['get'] = ['pass' if stat['n'] > 0 else 'fail' for stat in data['gets'] if stat['time'] == '5min']
        data['post'] = ['pass' if stat['n'] > 0 else 'fail' for stat in data['posts'] if stat['time'] == '5min']
        return data
