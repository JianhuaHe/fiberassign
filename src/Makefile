# Makefile for building fiber assignment tools
# variables for compiler flags, etc are propagated
# from the top-level Makefile.  DO NOT use this
# Makefile directly (build from the top level).

HEADERS = \
collision.h \
feat.h \
global.h \
misc.h \
structs.h

OBJS_SURVEYSIM = \
surveysim.o \
collision.o \
feat.o \
global.o \
misc.o \
structs.o

OBJS_FIBERASSIGN = \
fiberassign.o \
collision.o \
feat.o \
global.o \
misc.o \
structs.o

EXECUTABLES = fiberassign 
COMPILE = $(CXXFLAGS) $(OMPFLAGS) -I. $(CFITSIO_CPPFLAGS)
# valgrind
#COMPILE = -g -O0 -m64 -fPIC -DNDEBUG -std=c++11 $(OMPFLAGS) -I. $(CFITSIO_CPPFLAGS)
LINK = $(OMPFLAGS) $(CFITSIO)


all : $(EXECUTABLES)

install : all
	cp $(EXECUTABLES) $(INSTALL)

fiberassign : $(OBJS_FIBERASSIGN)
	$(CXX) -o $@ $^ $(LINK)

fiberassign_surveysim : $(OBJS_SURVEYSIM)
	$(CXX) -o $@ $^ $(LINK)


%.o : %.cpp $(HEADERS)
	$(CXX) $(COMPILE) -o $@ -c $<


clean :
	rm -f $(EXECUTABLES)
	rm -f *.o *~
	@cd $(INSTALL); rm -f $(EXECUTABLES)
