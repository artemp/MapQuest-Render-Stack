--
-- 
--
DROP INDEX IF EXISTS planet_osm_point_nature_index;
CREATE INDEX planet_osm_point_nature_index 
  ON planet_osm_point USING gist (way) 
  WHERE (leisure IS NOT NULL) OR 
        (landuse IS NOT NULL) OR 
        ("natural" IS NOT NULL) OR 
        (waterway IS NOT NULL);

