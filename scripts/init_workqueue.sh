source /data/mapquest/render_stack/scripts/env.sh
WORKQUEUE_DIR=/mqdata2/workqueue
rm -rf $WORKQUEUE_DIR/postgres/*
rm -rf $WORKQUEUE_DIR/logs/*
mkdir -p $WORKQUEUE_DIR/postgres
mkdir -p $WORKQUEUE_DIR/logs
initdb -D $WORKQUEUE_DIR/postgres
sed --in-place -e "s:#port = 5432:port = 5433:g" $WORKQUEUE_DIR/postgres/postgresql.conf
sed --in-place -e "s:#listen_addresses = 'localhost':listen_addresses = '$(hostname)':g" $WORKQUEUE_DIR/postgres/postgresql.conf
echo "host    all         all         64.0.0.0/8            trust" >>  $WORKQUEUE_DIR/postgres/pg_hba.conf

bash /data/mapquest/render_stack/scripts/start_workqueue_postgres.sh
sleep 5
createdb -p 5433 WorkQueue
psql -p 5433 -U mqmgr -d WorkQueue -c "create table tasks (gid bigint unique,  clientid int, x int, y int, z int, priority int, scheduled timestamp, completed timestamp, workerid text, status int, url text, style text);"
