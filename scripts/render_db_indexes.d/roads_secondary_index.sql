DROP INDEX IF EXISTS planet_osm_roads_secondary_index;
CREATE INDEX planet_osm_roads_secondary_index
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway = 'secondary';
