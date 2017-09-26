DIRS = $(filter-out ./.git, $(shell find . -mindepth 1 -maxdepth 1 -type d))

all:
	for i in $(DIRS); do $(MAKE) -C $$i; done

clean:
	for i in $(DIRS); do $(MAKE) -C $$i $@; done

test: all
	$(MAKE) -C encode_audio $@
	$(MAKE) -C encode_video $@
	$(MAKE) -C encoding $@
	$(MAKE) -C audio_decoding $@
	$(MAKE) -C audio_device $@
	$(MAKE) -C transcode_aac $@
	$(MAKE) -C transcoding $@
	$(MAKE) -C demuxing_decoding $@

