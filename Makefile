FUSE_CFLAGS = $(shell pkg-config --cflags fuse3)
FUSE_LIBS = $(shell pkg-config --libs fuse3)

CXXFLAGS := -Wall -Wextra -std=c++17 -O2 -flto -DNDEBUG
CXXFLAGS += $(FUSE_CFLAGS)
LIBS := -O2 -flto
LIBS += $(FUSE_LIBS)

CXX := g++
RM := rm -f

OBJ := Common.o
XXFSOBJ := BitmapAllocator.o FilePointer.o OpenedDir.o OpenedFile.o Xxfs.o XxfsMain.o
MKXXFSOBJ := MkXxfsMain.o
CLUXXOBJ := CluXxMain.o
ALL := xxfs mkxxfs cluxx

all: $(ALL)

xxfs: $(XXFSOBJ) $(OBJ)
	$(CXX) $(LIBS) -o $@ $^

mkxxfs: $(MKXXFSOBJ) $(OBJ)
	$(CXX) $(LIBS) -o $@ $^

cluxx: $(CLUXXOBJ) $(OBJ)
	$(CXX) $(LIBS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	$(RM) $(ALL) $(OBJ) $(XXFSOBJ) $(MKXXFSOBJ) $(CLUXXOBJ)
