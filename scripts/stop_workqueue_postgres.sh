
source /data/mapquest/render_stack/scripts/env.sh
WORKQUEUE_DIR=/mqdata2/workqueue
pg_ctl -D $WORKQUEUE_DIR/postgres -m f -l $WORKQUEUE_DIR/logs/postgres.log stop
