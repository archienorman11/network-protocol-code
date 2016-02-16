CONTIKI = ../..

CONTIKI_PROJECT = dtn

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
