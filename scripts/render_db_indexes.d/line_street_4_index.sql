DROP INDEX IF EXISTS planet_osm_line_street_4_index;
CREATE INDEX planet_osm_line_street_4_index
  ON planet_osm_line
  USING gist
  (way)
  WHERE highway IN ('secondary', 'tertiary');
