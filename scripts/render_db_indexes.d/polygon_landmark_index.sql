--
-- quite a long query here, to deal with the landmark layer. this renders
-- a bunch of different stuff and it's hard to separate into thematic 
-- layers. this is a mid-zoom index, so it would seem that a way_area 
-- filter would do well, but a little experimentation shows that this list
-- of tag values (derived from the stylesheet) is more selective. 
--
-- NOTE: this index will need to change any time the landmark query is
-- changed!
--
DROP INDEX IF EXISTS planet_osm_polygon_landmark_index;
CREATE INDEX planet_osm_polygon_landmark_index 
  ON planet_osm_polygon USING gist (way) 
  WHERE 
    (aeroway = ANY (ARRAY['aerodrome'::text, 
      'apron'::text, 
      'runway'::text])) OR 
    (amenity = ANY (ARRAY['college'::text, 
      'grave_yard'::text, 
      'hospital'::text, 
      'kindergarten'::text, 
      'parking'::text, 
      'school'::text, 
      'university'::text])) OR 
    (landuse = ANY (ARRAY['cemetery'::text, 
      'conservation'::text, 
      'forest'::text, 
      'grass'::text, 
      'grave_yard'::text, 
      'military'::text, 
      'recreation_ground'::text, 
      'residential'::text, 
      'retail'::text, 
      'wood'::text])) OR 
    (leisure = ANY (ARRAY['common'::text, 
      'golf_course'::text, 
      'nature_reserve'::text, 
      'park'::text, 
      'pitch'::text, 
      'playground'::text, 
      'recreation_ground'::text, 
      'sports_centre'::text, 
      'stadium'::text])) OR 
    (military = 'barracks'::text) OR 
    ("natural" = ANY (ARRAY['beach'::text, 
      'scrub'::text, 
      'wood'::text])) OR 
    (tourism = ANY (ARRAY['attraction'::text, 
      'zoo'::text]));
