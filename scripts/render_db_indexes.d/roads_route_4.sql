DROP INDEX IF EXISTS planet_osm_roads_route_4;
CREATE INDEX planet_osm_roads_route_4
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway IN ('motorway', 'trunk', 'primary', 'secondary') AND ref IS NOT NULL;
