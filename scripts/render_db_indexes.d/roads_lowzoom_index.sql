DROP INDEX IF EXISTS planet_osm_roads_lowzoom_index;
CREATE INDEX planet_osm_roads_lowzoom_index
  ON planet_osm_roads
  USING gist
  (way)
  WHERE highway IN ('motorway', 'trunk');
