DIRS = $(filter-out ./.git, $(shell find . -mindepth 1 -maxdepth 1 -type d))

all:
	for i in $(DIRS); do $(MAKE) -C $$i; done

clean:
	for i in $(DIRS); do $(MAKE) -C $$i $@; done
