"""Routes configuration

The more specific and detailed routes should be defined first so they
may take precedent over the more generic routes. For more information
refer to the routes manual at http://routes.groovie.org/docs/
"""
from routes import Mapper

def make_map(config):
    """Create, configure and return the routes Mapper"""
    map = Mapper(directory=config['pylons.paths']['controllers'],
                 always_scan=config['debug'])
    map.minimization = False
    map.explicit = False

    # The ErrorController route (handles 404/500 error pages); it should
    # likely stay at the top, ensuring it can always be resolved
    map.connect('/error/{action}', controller='error')
    map.connect('/error/{action}/{id}', controller='error')
    
    #mercator projection support - start

    map.connect('/{version:([0-9]+)}/{tile_type:[a-z_\-0-9]+}/{zoom:[0-9]+}/{tx:[0-9]+}/{ty:[0-9]+}.{extension:(jpg|jpeg|gif|png|json)}', controller='mercator_tiles', action='version_handler')

    #mercator projection support - end

    map.connect('/_stats.html', controller="stats", action="document_html")
    map.connect('/_stats.json', controller="stats", action="document_json")

    map.connect('/{controller}/{action}')
    map.connect('/{controller}/{action}/{id}')

    return map
