DIRS = $(filter-out ./.git, $(shell find . -mindepth 1 -maxdepth 1 -type d))

all:
	for i in $(DIRS); do $(MAKE) -C $$i || exit 1; done

clean:
	for i in $(DIRS); do $(MAKE) -C $$i $@; done

test: all
	$(MAKE) -C list_filters $@
	$(MAKE) -C encode_audio $@
	$(MAKE) -C encode_video $@
	$(MAKE) -C encode_stream $@
	$(MAKE) -C encoding $@
	$(MAKE) -C decode_audio $@
	$(MAKE) -C decode_stream $@
	$(MAKE) -C audio_decoding $@
	$(MAKE) -C remuxing $@
	$(MAKE) -C transcode_aac $@
	$(MAKE) -C transcoding $@
	$(MAKE) -C demuxing_decoding $@
	$(MAKE) -C read_metadata $@

interactive-test:
	$(MAKE) -C audio_device $@
	$(MAKE) -C player $@
