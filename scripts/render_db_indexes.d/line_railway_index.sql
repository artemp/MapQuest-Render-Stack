--
-- speeds up railway drawing, since railways are comparitively
-- infrequent in the line table compared to roads and paths and so
-- forth.
--
DROP INDEX IF EXISTS planet_osm_line_railway_index;
CREATE INDEX planet_osm_line_railway_index 
  ON planet_osm_line USING gist (way) 
  WHERE (railway IS NOT NULL);

