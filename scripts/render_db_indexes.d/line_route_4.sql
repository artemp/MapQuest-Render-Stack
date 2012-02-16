DROP INDEX IF EXISTS planet_osm_line_route_4;
CREATE INDEX planet_osm_line_route_4
  ON planet_osm_line
  USING gist
  (way)
  WHERE highway IN ('motorway', 'trunk', 'primary', 'secondary', 'tertiary') AND ref IS NOT NULL;
