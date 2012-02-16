DROP INDEX IF EXISTS planet_osm_roads_route_3;
CREATE INDEX planet_osm_roads_route_3
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway IN ('motorway', 'trunk', 'primary') AND ref IS NOT NULL;
