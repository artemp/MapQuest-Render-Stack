--
-- looking through the query log showed up this statement as occasionally 
-- taking a very large amount of time.
--
DROP INDEX IF EXISTS planet_osm_polygon_water_index;
CREATE INDEX planet_osm_polygon_water_index
  ON planet_osm_polygon USING gist (way)
  WHERE waterway IN ('dock','mill_pond','riverbank','canal') 
     OR landuse IN ('reservoir','water','basin') 
     OR "natural" IN ('lake','water','land','marsh','scrub','wetland','glacier');
