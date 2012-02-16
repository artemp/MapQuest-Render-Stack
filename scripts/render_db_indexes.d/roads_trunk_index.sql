DROP INDEX IF EXISTS planet_osm_roads_trunk_index;
CREATE INDEX planet_osm_roads_trunk_index
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway = 'trunk';
