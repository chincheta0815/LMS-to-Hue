DEPS_DIR	= /Users/weiler/devel.env/deps.dir
DEPS_INC_DIR	= $(DEPS_DIR)/include
DEPS_LIB_DIR	= $(DEPS_DIR)/lib
BASE		= ..
OS 		:= $(shell uname)

ifeq ($(OS),SunOS) 
LINKSTATIC	=
else ifeq ($(OS),Darwin) 
LINKSTATIC	=
else
LINKSTATIC 	= -static
endif

LIBOPENSSL      = $(DEPS_INC_DIR)
LIBCURL		= $(DEPS_INC_DIR)
HUELIBENT	= $(DEPS_INC_DIR)/libHueEnt
JSONC		= $(DEPS_INC_DIR)
ALAC            = $(DEPS_INC_DIR)/alac
OPUS            = $(DEPS_INC_DIR)/opus
OGG             = $(DEPS_INC_DIR)/ogg
VORBIS          = $(DEPS_INC_DIR)/vorbis
SQUEEZE2HUE	= ./squeeze2hue
SQUEEZETINY	= ./squeezetiny
TOOLS		= ./tools
LIBUPNP         = $(DEPS_INC_DIR)/upnp
MDNSSD		= $(BASE)/mdnssd
HUEBRIDGE       = ./huebridge

DEFINES 	+= -DHAVE_STDINT_H -DRESAMPLE -DNDEBUG -D_FILE_OFFSET_BITS=64 

vpath %.c $(MDNSSD):$(SQUEEZE2HUE):$(SQUEEZETINY):$(TOOLS):$(HUEBRIDGE)

LIBRARY 	= $(DEPS_LIB_DIR)/libixml.a $(DEPS_LIB_DIR)/libsoxr.a $(DEPS_LIB_DIR)/libalac.a
                  
LIBRARY_STATIC 	= $(LIBRARY) $(DEPS_LIB_DIR)/libssl.a $(DEPS_LIB_DIR)/libcrypto.a \
                  $(DEPS_LIB_DIR)/libcurl.a $(DEPS_LIB_DIR)/libjson-c.a $(DEPS_LIB_DIR)/libHueEnt.a \
                  $(DEPS_LIB_DIR)/libfaad.a $(DEPS_LIB_DIR)/libFLAC.a $(DEPS_LIB_DIR)/libmad.a \
                  $(DEPS_LIB_DIR)/libogg.a \
                  $(DEPS_LIB_DIR)/libvorbis.a $(DEPS_LIB_DIR)/libvorbisfile.a \
                  $(DEPS_LIB_DIR)/libopus.a $(DEPS_LIB_DIR)/libopusfile.a \
                  $(DEPS_LIB_DIR)/libfftw3.a

INCLUDE = -I$(SQUEEZETINY) \
	  -I$(SQUEEZE2HUE)/inc \
          -I$(TOOLS) \
          -I$(LIBUPNP) \
          -I$(MDNSSD) \
          -I$(HUELIBENT) \
          -I$(JSONC) \
          -I$(OGG) \
          -I$(OPUS) \
          -I$(VORBIS) \
          -I$(LIBCURL) \
          -I$(HUEBRIDGE) \
          -I$(ALAC)

DEPS	= $(SQUEEZETINY)/squeezedefs.h
		  
SOURCES = conf_util.c hue_bridge.c hue_analyze.c hue_stream.c \
          log_util.c mdnssd-min.c squeeze2hue.c util.c \
          buffer.c decode.c main.c output.c output_huebridge.c output_pack.c \
          pcm.c process.c resample.c slimproto.c stream.c utils.c util_common.c
		
SOURCES_LIBS = alac.c faad.c flac.c mad.c mpg.c opus.c vorbis.c
		
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

