SRC = $(wildcard *.vert *.frag)
INC = $(SRC:%=inc/%.inc)

all: $(INC)

inc/%.vert.inc: %.vert
	makesh $^
inc/%.frag.inc: %.frag
	makesh $^
