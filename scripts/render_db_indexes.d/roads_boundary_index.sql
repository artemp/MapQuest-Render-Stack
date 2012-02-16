--
-- admin levels 2 & 4 are drawn at very low zooms, so this index lets us pull
-- that data out much faster, given the area which must be searched.
--
DROP INDEX IF EXISTS planet_osm_roads_boundary_index;
CREATE INDEX planet_osm_roads_boundary_index 
  ON planet_osm_roads USING gist (way) 
  WHERE 
    ((boundary = 'administrative'::text) AND 
     (NOT (boundary = 'maritime'::text))) 
  AND (admin_level = ANY (ARRAY['2'::text, '4'::text]));
