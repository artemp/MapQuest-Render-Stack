export ROOT_DIR=/data/mapquest/render_stack
export DATA_DIR=$ROOT_DIR/render_stack_data
export RUNTIME=$ROOT_DIR/runtime
export PATH=$RUNTIME/bin:$RUNTIME/apache2.2/bin:$PATH
export LD_LIBRARY_PATH=$RUNTIME/lib:$RUNTIME/lib64:$LD_LIBRARY_PATH:$RUNTIME/lib/mysql/:$RUNTIME/apache2.2/lib
export PERL5LIB=/mqdata/render_stack/runtime/lib/perl5/5.8.8/:/mqdata/render_stack/runtime/lib64/perl5/5.8.8/:/mqdata/render_stack/runtime/lib/perl5/site_perl/5.8.8:/mqdata/render_stack/runtime/lib64/perl5/site_perl/5.8.8/x86_64-linux-thread-multi/
export PYTHONPATH=$RUNTIME/python:$RUNTIME/lib/python/:$RUNTIME/lib/python2.6/site-packages:/mqdata/render_stack/bin/gcc-4.1.2/debug/:$PYTHONPATH
export BOOST_ROOT=$RUNTIME/boost_1_45_0/
