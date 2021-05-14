NNMAKEFILE_HEADERS = nn.h inproc.h ipc.h tcp.h pair.h pubsub.h reqrep.h \
	pipeline.h survey.h bus.h

NANOMSG_HEADERS = $(addprefix $(LIBS_DIR)/nanomsg/src/,$(NNMAKEFILE_HEADERS))
NANOMSG_INCLUDE_DIR = $(LIBS_INCLUDE_DIR)/nanomsg
EXTERN_INCLUDES = $(NANOMSG_INCLUDE_DIR)

$(LIBS_DIR)/nanomsg/Makefile: $(LIBS_DIR)/nanomsg/Makefile.in
	@cd $(^D) && ./configure

$(NANOMSG_INCLUDE_DIR): $(LIBS_DIR)/nanomsg/Makefile.in
	@echo + $@
	@mkdir -p $(NANOMSG_INCLUDE_DIR)
	@for header in $(NANOMSG_HEADERS); do \
	  cp $$header $(NANOMSG_INCLUDE_DIR); \
	done

$(LIBS_DIR)/libnanomsg.a: $(LIBS_DIR)/nanomsg/Makefile
	@echo + $@ [make $^]
	@cd $(^D) && aclocal && automake
	@make -C $(^D) -j8
	@cp $(^D)/.libs/libnanomsg.a $(LIBS_DIR)/
