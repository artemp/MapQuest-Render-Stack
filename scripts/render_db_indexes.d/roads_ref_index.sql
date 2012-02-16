--
-- when drawing roads with references, it's slow to pick 
-- through all the ones which don't have references. this
-- should allow us to access them more easily.
-- 
DROP INDEX IF EXISTS planet_osm_roads_route_1;
CREATE INDEX planet_osm_roads_route_1
  ON planet_osm_roads USING gist(way)
  WHERE highway = 'motorway' AND ref IS NOT NULL;
  
DROP INDEX IF EXISTS planet_osm_roads_route_2;
CREATE INDEX planet_osm_roads_route_2
  ON planet_osm_roads USING gist(way)
  WHERE highway IN ('motorway', 'trunk') AND ref IS NOT NULL;
  
DROP INDEX IF EXISTS planet_osm_roads_route_3;
CREATE INDEX planet_osm_roads_route_3
  ON planet_osm_roads USING gist(way)
  WHERE highway IN ('motorway', 'trunk', 'primary') AND ref IS NOT NULL;
  
DROP INDEX IF EXISTS planet_osm_roads_route_4;
CREATE INDEX planet_osm_roads_route_4
  ON planet_osm_roads USING gist(way)
  WHERE highway IN ('motorway', 'trunk', 'primary', 'secondary') AND ref IS NOT NULL;

