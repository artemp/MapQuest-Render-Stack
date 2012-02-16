-- 
-- motorways are another feature which is drawn at lower zooms than most 
-- other features.
--
DROP INDEX IF EXISTS planet_osm_roads_motorway_index;
CREATE INDEX planet_osm_roads_motorway_index 
  ON planet_osm_roads USING gist (way) 
  WHERE (highway = 'motorway'::text);

DROP INDEX IF EXISTS planet_osm_roads_lowzoom_index;
CREATE INDEX planet_osm_roads_lowzoom_index
  ON planet_osm_roads USING gist(way)
  WHERE highway IN ('motorway', 'trunk');

DROP INDEX IF EXISTS planet_osm_roads_motorway_interchange_index;
CREATE INDEX planet_osm_roads_motorway_interchange_index
  ON planet_osm_roads USING gist(way)
  WHERE highway = 'motorway_link';

DROP INDEX IF EXISTS planet_osm_roads_trunk_index;
CREATE INDEX planet_osm_roads_trunk_index
  ON planet_osm_roads USING gist(way)
  WHERE highway = 'trunk';

DROP INDEX IF EXISTS planet_osm_roads_primary_index;
CREATE INDEX planet_osm_roads_primary_index
  ON planet_osm_roads USING gist(way)
  WHERE highway = 'primary';

DROP INDEX IF EXISTS planet_osm_roads_secondary_index;
CREATE INDEX planet_osm_roads_secondary_index
  ON planet_osm_roads USING gist(way)
  WHERE highway = 'secondary';

