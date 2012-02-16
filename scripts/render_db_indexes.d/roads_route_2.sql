DROP INDEX IF EXISTS planet_osm_roads_route_2;
CREATE INDEX planet_osm_roads_route_2
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway IN ('motorway', 'trunk') AND ref IS NOT NULL;
