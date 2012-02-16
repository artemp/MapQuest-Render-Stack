source /mqdata/pg_render_stack/scripts/env.sh
nohup /mqdata/render_stack/rendermq/bin/gcc-4.1.2/release/tile_broker /srv/planet/config/dqueue.conf >&/srv/planet/mongrel2/logs/tile_broker.log & 
