DROP INDEX IF EXISTS planet_osm_line_tertiary_index;
CREATE INDEX planet_osm_line_tertiary_index
  ON planet_osm_line
  USING gist
  (way)
  WHERE highway IN ('tertiary', 'residential', 'living_street', 'unclassified', 'road', 'service');
