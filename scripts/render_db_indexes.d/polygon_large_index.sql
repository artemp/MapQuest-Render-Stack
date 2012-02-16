--
-- for low zooms, the style shows only large ways. however the query area at
-- low zooms is huge. a partial index on the way_area attribute solves this
-- nicely.
--
DROP INDEX IF EXISTS planet_osm_polygon_large_index;
CREATE INDEX planet_osm_polygon_large_index 
  ON planet_osm_polygon USING gist (way) 
  WHERE (way_area > (200000)::double precision);

