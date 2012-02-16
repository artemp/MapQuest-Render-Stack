--
-- speeds up airport features, since they're very rare and yet 
-- quite large and displayed at a fairly low zoom.
--
DROP INDEX IF EXISTS planet_osm_line_runway_index;
CREATE INDEX planet_osm_line_runway_index 
  ON planet_osm_line USING gist (way) 
  WHERE (aeroway = ANY (ARRAY['runway'::text, 'taxiway'::text]));

