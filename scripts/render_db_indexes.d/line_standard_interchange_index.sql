DROP INDEX IF EXISTS planet_osm_line_standard_interchange_index;
CREATE INDEX planet_osm_line_standard_interchange_index
  ON planet_osm_line
  USING gist
  (way)
  WHERE highway IN ('trunk_link', 'primary_link', 'secondary_link', 'tertiary_link', 'residential_link', 'unclassified_link');
