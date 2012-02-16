DROP INDEX IF EXISTS planet_osm_roads_primary_index;
CREATE INDEX planet_osm_roads_primary_index
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway = 'primary';
