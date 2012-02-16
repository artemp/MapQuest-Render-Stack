"""The application's Globals object"""

from beaker.cache import CacheManager
from beaker.util import parse_cache_config_options
from storage_node_pylons.lib.mqStats import mqStats
from storage_node_pylons.lib.mqExpiry import mqExpiryInfo

class Globals(object):
    """Globals acts as a container for objects available throughout the
    life of the application

    """

    def __init__(self, config):
        """One instance of Globals is created during application
        initialization and is available during requests via the
        'app_globals' variable

        """
        self.cache = CacheManager(**parse_cache_config_options(config))

        self._statsDB = None
        self.stats_collector_host = config['app_conf']['stats_collector.host']
        self.stats_collector_port = config['app_conf']['stats_collector.port']

        self._expiryDB = None
        self.expiry_info_host = config['app_conf']['expiry_info.host']
        self.expiry_info_port = config['app_conf']['expiry_info.port']

        self.cacheInfo = dict()
	for k, v in config['app_conf'].iteritems():
		if k.startswith('versions.'):
			self.cacheInfo[k[9:]] = { "root" : v }
        
    def statsDB(self):
        if self._statsDB is None:
            self._statsDB = mqStats(self.stats_collector_host, self.stats_collector_port)
        return self._statsDB

    def expiryDB(self):
        if self._expiryDB is None:
            self._expiryDB = mqExpiryInfo(self.expiry_info_host, self.expiry_info_port)
        return self._expiryDB
