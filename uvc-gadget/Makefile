SUBDIRS = applications/core \
          applications/sei_streaming \
          applications/sei_streaming_Advanced \
          applications/smart_streaming \
          tools/sei_extractor

all:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

.PHONY: all clean $(SUBDIRS)
