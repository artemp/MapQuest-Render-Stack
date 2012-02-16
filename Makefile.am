ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = tile_handler tile_broker broker_ctl expire_tiles tile_submitter
lib_LTLIBRARIES = \
	librendermq_logging.la librendermq_proto.la librendermq_dqueue.la \
	librendermq_http.la librendermq_storage.la 

BOOST_LIBS=$(BOOST_LDFLAGS) \
	$(BOOST_DATE_TIME_LIB) \
	$(BOOST_FILESYSTEM_LIB) \
	$(BOOST_IOSTREAMS_LIB) \
	$(BOOST_PROGRAM_OPTIONS_LIB) \
	-l$(BOOST_PYTHON_LIB) \
	$(BOOST_REGEX_LIB) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_THREAD_LIB)

librendermq_logging_la_SOURCES = \
	logging/file_logger.cpp \
	logging/logger.cpp \
	logging/stdout_logger.cpp 
librendermq_logging_la_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
librendermq_logging_la_LIBADD = $(DEPS_LIBS) $(BOOST_LIBS)

librendermq_proto_la_SOURCES = \
	tile_utils.cpp \
	zstream_pbuf.cpp \
	zstream.cpp \
	zmq_utils.cpp \
	storage/meta_tile.cpp 
nodist_librendermq_proto_la_SOURCES = proto/tile.pb.h proto/tile.pb.cc
BUILT_SOURCES = proto/tile.pb.h proto/tile.pb.cc
CLEANFILES = proto/tile.pb.h proto/tile.pb.cc
librendermq_proto_la_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
librendermq_proto_la_LIBADD = $(DEPS_LIBS) $(BOOST_LIBS) librendermq_logging.la

proto/tile.pb.cc proto/tile.pb.h: proto/tile.proto
	@echo " PROTOC $<"; $(PROTOC) -I$(@D) --cpp_out=$(@D) $<

librendermq_dqueue_la_SOURCES = \
	dqueue/distributed_queue.cpp \
	dqueue/distributed_queue_config.cpp \
	dqueue/zmq_backend.cpp \
	dqueue/consistent_hash.cpp \
	dqueue/backend.cpp \
	dqueue/pgq_backend.cpp 
librendermq_dqueue_la_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
librendermq_dqueue_la_LIBADD = $(DEPS_LIBS) $(BOOST_LIBS) librendermq_logging.la librendermq_proto.la

librendermq_http_la_SOURCES = \
	http/http_date_parser.cpp \
	http/http.cpp \
	http/http_reply.cpp
librendermq_http_la_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
librendermq_http_la_LIBADD = $(DEPS_LIBS) $(BOOST_LIBS)

librendermq_storage_la_SOURCES = \
	image/image.cpp \
	storage/simple_http_storage.cpp \
	storage/null_storage.cpp \
	storage/compositing_storage.cpp \
	storage/per_style_storage.cpp \
	storage/tile_storage.cpp \
	storage/hashwrapper.cpp \
	storage/union_storage.cpp \
	storage/null_handle.cpp \
	storage/http_storage.cpp \
	storage/disk_storage.cpp \
	storage/lts_storage.cpp 
librendermq_storage_la_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
librendermq_storage_la_LIBADD = $(DEPS_LIBS) $(BOOST_LIBS) librendermq_proto.la librendermq_http.la

tile_handler_SOURCES = \
	mongrel_request.cpp \
	tile_path_parser.cpp \
	mongrel_request_parser.cpp \
	storage_worker.cpp \
	tile_handler_main.cpp \
	tile_handler.cpp 
tile_handler_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
tile_handler_LDADD = \
	librendermq_logging.la \
	librendermq_proto.la \
	librendermq_dqueue.la \
	librendermq_http.la \
	librendermq_storage.la \
	$(DEPS_LIBS) $(BOOST_LIBS)

broker_ctl_SOURCES = \
	broker_ctl.cpp
broker_ctl_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
broker_ctl_LDADD = \
	librendermq_logging.la \
	librendermq_proto.la \
	librendermq_dqueue.la \
	$(DEPS_LIBS) $(BOOST_LIBS)

expire_tiles_SOURCES = \
	expire_tiles.cpp 
expire_tiles_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
expire_tiles_LDADD = \
	librendermq_logging.la \
	librendermq_proto.la \
	librendermq_dqueue.la \
	librendermq_http.la \
	librendermq_storage.la \
	$(DEPS_LIBS) $(BOOST_LIBS) 

tile_broker_SOURCES = \
	tile_broker.cpp \
	tile_broker_impl.cpp
tile_broker_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
tile_broker_LDADD = \
	librendermq_logging.la \
	librendermq_proto.la \
	librendermq_dqueue.la \
	$(DEPS_LIBS) $(BOOST_LIBS)

tile_submitter_SOURCES = \
	tile_submitter.cpp 
tile_submitter_CPPFLAGS = $(DEPS_CFLAGS) $(BOOST_CPPFLAGS)
tile_submitter_LDADD = \
	librendermq_logging.la \
	librendermq_proto.la \
	librendermq_dqueue.la \
	librendermq_http.la \
	librendermq_storage.la \
	$(DEPS_LIBS) $(BOOST_LIBS) 

#	storage/tile_storage_python.cpp \
#	dqueue/distributed_queue_python.cpp \
#	logging/python.cpp

#	storage/expiry_overlay.cpp \
#	storage/expiry_service.cpp \
#	storage/expiry_server_main.cpp \
#	storage/expiry_server.cpp 
