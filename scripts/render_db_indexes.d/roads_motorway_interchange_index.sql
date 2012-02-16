DROP INDEX IF EXISTS planet_osm_roads_motorway_interchange_index;
CREATE INDEX planet_osm_roads_motorway_interchange_index
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway = 'motorway_link';
