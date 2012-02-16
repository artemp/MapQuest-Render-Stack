--
-- waterways are similarly drawn at lower zooms, and in many places
-- there are quite a lot of them.
--
DROP INDEX IF EXISTS planet_osm_line_waterway_index;
CREATE INDEX planet_osm_line_waterway_index 
  ON planet_osm_line USING gist (way) 
  WHERE (waterway = ANY (ARRAY[
    'weir'::text, 
    'river'::text, 
    'canal'::text, 
    'derelict_canal'::text, 
    'stream'::text, 
    'drain'::text, 
    'ditch'::text
  ]));
