DROP INDEX IF EXISTS planet_osm_line_route_5;
CREATE INDEX planet_osm_line_route_5
  ON planet_osm_line
  USING gist
  (way)
  WHERE highway IN ('motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'residential', 'unclassified', 'road') AND ref IS NOT NULL;
