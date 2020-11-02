MAKE=make
RM=rm
MKDIR=mkdir

# -----Begin user-editable area-----

# -----End user-editable area-----

# If no configuration is specified, "Debug" will be used
ifndef CFG
CFG=Debug
endif

#
# Configuration: Debug
#
ifeq "$(CFG)" "Debug"
OUTDIR=Debug
OUTFILE=$(OUTDIR)/librtmp.a
CFG_INC=-I../../common/log -I../../lib/zlog/include -I../../lib/libadns/include
CFG_LIB=
CFG_OBJ=
COMMON_OBJ=$(OUTDIR)/amf.o $(OUTDIR)/hashswf.o \
	$(OUTDIR)/log.o $(OUTDIR)/md5.o $(OUTDIR)/parseurl.o $(OUTDIR)/cencode.o $(OUTDIR)/rtmp.o
OBJ=$(COMMON_OBJ) $(CFG_OBJ)
ALL_OBJ=$(OUTDIR)/amf.o $(OUTDIR)/hashswf.o \
	$(OUTDIR)/log.o $(OUTDIR)/md5.o $(OUTDIR)/parseurl.o $(OUTDIR)/cencode.o $(OUTDIR)/rtmp.o

COMPILE=arm-hisiv400-linux-gcc -c -g -DDebug $(CFG_LIB) -o "$(OUTDIR)/$(*F).o" $(CFG_INC) "$<"
LINK=arm-hisiv400-linux-ar -rs  "$(OUTFILE)" $(OBJ)

# Pattern rules
$(OUTDIR)/%.o : ./%.c
	$(COMPILE)

# Build rules
all: $(OUTFILE)

$(OUTFILE): $(OUTDIR)  $(OBJ)
	$(LINK)

$(OUTDIR):
	$(MKDIR) -p "$(OUTDIR)"

# Rebuild this project
rebuild: cleanall all

# Clean this project
clean:
	$(RM) -f $(OUTFILE)
	$(RM) -f $(OBJ)

# Clean this project and all dependencies
cleanall: clean
endif

#
# Configuration: Release
#
ifeq "$(CFG)" "Release"
OUTDIR=Release
OUTFILE=$(OUTDIR)/libAudioProcess.a
CFG_INC=-Isrc/ffmpeg/arm-linux/include -Isrc/speex/include -Isrc -I../../../common/log -I../../lib/libadns/include
CFG_LIB=-Lsrc/ffmpeg/arm-linux/lib/Release -lavutil -lavcodec -lavfilter -lavformat -lfdk-aac -lswresample -lx264 -Lspeex/arm-linux/lib/Release -lspeexdsp -lpthread -L../libadns/lib -lands
CFG_OBJ=
COMMON_OBJ=$(OUTDIR)/AudioProcess.o $(OUTDIR)/Log.o \
	$(OUTDIR)/Media.o 
OBJ=$(COMMON_OBJ) $(CFG_OBJ)
ALL_OBJ=$(OUTDIR)/AudioProcess.o $(OUTDIR)/Log.o \
	$(OUTDIR)/Media.o 

COMPILE=arm-hisiv400-linux-gcc -c $(CFG_LIB) -o "$(OUTDIR)/$(*F).o" $(CFG_INC) "$<"
LINK=arm-hisiv400-linux-ar -rs  "$(OUTFILE)" $(OBJ)

# Pattern rules
$(OUTDIR)/%.o : src/%.c
	$(COMPILE)

# Build rules
all: $(OUTFILE)

$(OUTFILE): $(OUTDIR)  $(OBJ)
	$(LINK)

$(OUTDIR):
	$(MKDIR) -p "$(OUTDIR)"

# Rebuild this project
rebuild: cleanall all

# Clean this project
clean:
	$(RM) -f $(OUTFILE)
	$(RM) -f $(OBJ)

# Clean this project and all dependencies
cleanall: clean
endif
