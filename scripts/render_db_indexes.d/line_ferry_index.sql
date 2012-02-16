--
-- ferries are drawn at low zoom, so it's necessary to pull them out
-- of the normal index and single them out for special attention.
--
DROP INDEX IF EXISTS planet_osm_line_ferry_index;
CREATE INDEX planet_osm_line_ferry_index 
  ON planet_osm_line USING gist (way) 
  WHERE (route = 'ferry'::text);

