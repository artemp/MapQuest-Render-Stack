--
-- 
--
DROP INDEX IF EXISTS planet_osm_point_place_index;
CREATE INDEX planet_osm_point_place_index 
  ON planet_osm_point USING gist (way) 
  WHERE (place = ANY (ARRAY[
    'city'::text, 
    'continent'::text, 
    'country'::text, 
    'county'::text, 
    'hamlet'::text, 
    'large_town'::text, 
    'large_village'::text, 
    'locality'::text, 
    'metropolis'::text, 
    'small_town'::text, 
    'state'::text, 
    'suburb'::text, 
    'town'::text, 
    'village'::text
  ]));
