--
-- streets drawn at mid-zoom levels need some indexing support, as there's a 
-- lot of stuff in the _line table to pick through to get at the ones which
-- are "interesting" at that zoom.
--

DROP INDEX IF EXISTS planet_osm_line_route_4;
CREATE INDEX planet_osm_line_route_4
  ON planet_osm_line USING gist(way)
  WHERE highway IN ('motorway', 'trunk', 'primary', 'secondary', 'tertiary') AND ref IS NOT NULL;

DROP INDEX IF EXISTS planet_osm_line_route_5;
CREATE INDEX planet_osm_line_route_5
  ON planet_osm_line USING gist(way)
  WHERE highway IN ('motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'residential', 'unclassified', 'road') AND ref IS NOT NULL;

DROP INDEX IF EXISTS planet_osm_line_street_4_index;
CREATE INDEX planet_osm_line_street_4_index
  ON planet_osm_line USING gist(way)
  WHERE highway IN ('secondary', 'tertiary');

DROP INDEX IF EXISTS planet_osm_line_tertiary_index;
CREATE INDEX planet_osm_line_tertiary_index
  ON planet_osm_line USING gist(way)
  WHERE highway IN ('tertiary', 'residential', 'living_street', 'unclassified', 'road', 'service');

DROP INDEX IF EXISTS planet_osm_line_standard_interchange_index;
CREATE INDEX planet_osm_line_standard_interchange_index
  ON planet_osm_line USING gist(way)
  WHERE highway IN ('trunk_link', 'primary_link', 'secondary_link', 'tertiary_link', 'residential_link', 'unclassified_link');
