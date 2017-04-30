BASE		= ..
OS 		:= $(shell uname)

ifeq ($(OS),SunOS) 
LINKSTATIC	=
else ifeq ($(OS),Darwin) 
LINKSTATIC	=
else
LINKSTATIC 	= -static
endif

TOOLS		= ./tools
SRC		= ./squeeze2hue
SQUEEZETINY	= ./squeezetiny

LIBHUEC		= ./libhuec

VALGRIND	= $(BASE)/valgrind
UPNP 		= $(BASE)/libupnp/1.6.19
JANSSON		= $(BASE)/jansson/2.10

DEFINES 	+= -DHAVE_STDINT_H -DRESAMPLE -DNDEBUG -D_FILE_OFFSET_BITS=64 

vpath %.c $(TOOLS):$(SRC):$(LIBHUEC):$(SQUEEZETINY)

LIBRARY 	= $(OBJ)/libupnp.a $(OBJ)/libthreadutil.a $(OBJ)/libixml.a $(OBJ)/libsoxr.a $(OBJ)/libmad.a $(OBJ)/libjansson.a
LIBRARY_STATIC 	= $(LIBRARY) $(OBJ)/libfaad.a $(OBJ)/libsoxr.a $(OBJ)/libFLAC.a $(OBJ)/libmad.a $(OBJ)/libjansson.a

INCLUDE = -I. \
	  -I$(SQUEEZETINY) \
	  -I$(TOOLS) \
	  -I$(SRC) -I$(SRC)/inc \
	  -I$(VALGRIND)/memcheck -I$(VALGRIND)/include \
	  -I$(UPNP)/ixml/inc -I$(UPNP)/upnp/inc \
	  -I$(BASE)/mpg123/src/libmpg123\
	  -I$(BASE)/flac/include\
	  -I$(BASE)/faad2/include\
	  -I$(BASE)/libmad\
	  -I$(BASE)/soxr/src\
	  -I$(UPNP)/ixml/inc -I$(UPNP)/upnp/inc -I$(UPNP)/build/inc -I$(UPNP)/threadutil/inc \
	  -I$(JANSSON)/include \
	  -I$(LIBHUEC)/include

DEPS	= $(SQUEEZETINY)/squeezedefs.h
		  
SOURCES = slimproto.c buffer.c stream.c utils.c pcm.c process.c resample.c \
	output.c decode.c output_pack.c output_hue.c  main.c util_common.c \
      	squeeze2hue.c util.c conf_util.c log_util.c \
	hue_capabilities.c hue_configuration.c hue_interface.c hue_lights.c
		
SOURCES_LIBS = mpg.c flac.c mad.c faad.c
		
OBJECTS 	= $(patsubst %.c,$(OBJ)/%.o,$(SOURCES)) $(patsubst %.c,$(OBJ)/%.o,$(SOURCES_LIBS)) 
OBJECTS_STATIC 	= $(patsubst %.c,$(OBJ)/%.o,$(SOURCES)) $(patsubst %.c,$(OBJ)/%-static.o,$(SOURCES_LIBS)) 

all: $(EXECUTABLE) $(EXECUTABLE_STATIC)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBRARY) $(LDFLAGS) -o $@

$(EXECUTABLE_STATIC): $(OBJECTS_STATIC)
	$(CC) $(OBJECTS_STATIC) $(LIBRARY_STATIC) $(LDFLAGS) $(LINKSTATIC) -o $@	

$(OBJECTS): $(DEPS) | bin $(OBJ)

$(OBJ):
	@mkdir -p $@
	
bin:	
	@mkdir -p bin

$(OBJ)/%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDE) $< -c -o $@

$(OBJ)/%-static.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLINKALL $(INCLUDE) $< -c -o $(OBJ)/$*-static.o	
	
clean:
	rm -f $(OBJECTS) $(OBJECTS_STATIC) $(EXECUTABLE) $(EXECUTABLE_STATIC)

