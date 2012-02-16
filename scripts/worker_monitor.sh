cd /data/mapquest/render_stack/scripts/
worker_count=$(ps aux | grep worker | grep -v grep | grep -v worker_monitor |  wc -l )
min_worker_count=9
worker_count=$((worker_count/2))
next_worker_id=$((worker_count+1))
if [ $worker_count -lt $min_worker_count ];
then
	echo Found $worker_count workers, minimum number of required workers is $min_worker_count
	echo Starting a new worker with id of $next_worker_id
	bash ./start_hss_worker.sh $next_worker_id
else
	echo Found $worker_count workers, minimum number of required worker is $min_worker_count, not starting a new worker
fi
