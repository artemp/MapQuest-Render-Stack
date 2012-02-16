DROP INDEX IF EXISTS planet_osm_roads_route_1;
CREATE INDEX planet_osm_roads_route_1
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway = 'motorway' AND ref IS NOT NULL;
