#Written by Will Sumner
#Create parsing program
#yaccprog.out: parser.tab.c lex.yy.c ast.c ast.h symtable.c symtable.h
#	$(CC) -o yaccprog.out parser.tab.c lex.yy.c ast.c symtable.c -ll -ly

CC        := gcc # compiler
CPP        := g++ # compiler
MKDIR_P   := mkdir -p # mkdir command

CFLAGS    := -Wall -D_GNU_SOURCE -std=c++11# cflags
DFLAGS    := -g -DDEBUG -O0 # debug flags
LDFLAGS   := # lex and yacc flags

# dir names
RELDIR    := release
DEBDIR    := debugging

# For building project
REGFILES  := monitor circ_buff # all handwritten src files
ALLFILES  := $(REGFILES) # all needed files
OBJFILES  := $(addprefix $(RELDIR)/, $(addsuffix .o,$(ALLFILES)))
DOBJFILES := $(addprefix $(DEBDIR)/, $(addsuffix .debug.o,$(ALLFILES)))

# For making a tar
CPPFILES    := $(addsuffix .cpp,$(REGFILES))
HFILES    := $(addsuffix .h,$(REGFILES))
SHFILES   := #compile.sh run.sh
TARFILES  := $(CPPFILES) $(HFILES) $(SHFILES) Makefile
PREPOUT   := monitor.tar

TARGET    := monitor
DTARGET   := monitordebug
.PHONY    := clean tar

all: $(TARGET)
debug: $(DTARGET)
tar: $(PREPOUT)

# Tar when any src files change or prep script changes
$(PREPOUT): $(TARFILES)
	tar -cvf $(PREPOUT) $(TARFILES)

$(TARGET): $(OBJFILES)
	$(CPP) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJFILES)

$(DTARGET): $(DOBJFILES)
	$(CPP) $(CFLAGS) $(LDFLAGS) $(DFLAGS) -o $(DTARGET) $(DOBJFILES)

$(OBJFILES):  | $(RELDIR)
$(DOBJFILES): | $(DEBDIR)

$(RELDIR):
	${MKDIR_P} $(RELDIR)

$(DEBDIR):
	${MKDIR_P} $(DEBDIR)


###################################
#########    RELEASE    ###########
###################################

$(RELDIR)/monitor.o: monitor.cpp monitor.h circ_buff.h circ_buff.cpp
	$(CPP) $(CFLAGS) -c monitor.cpp -o $@

$(RELDIR)/circ_buff.o: circ_buff.cpp circ_buff.h
	$(CPP) $(CFLAGS) -c circ_buff.cpp -o $@


###################################
#########     DEBUG     ###########
###################################

$(DEBDIR)/monitor.debug.o: monitor.cpp monitor.h circ_buff.cpp circ_buff.h
	$(CPP) $(CFLAGS) $(DFLAGS) -c monitor.cpp -o $@

$(DEBDIR)/circ_buff.debug.o: circ_buff.cpp circ_buff.h
	$(CPP) $(CFLAGS) $(DFLAGS) -c circ_buff.cpp -o $@

clean:
	-@rm -rf $(RELDIR)/ $(DEBDIR)/ 2>/dev/null || true # 
	-@rm $(TARGET) $(DTARGET)  2>/dev/null || true # remove targets
	-@rm $(PREPOUT) 2>/dev/null || true # remove tar file
	-@rm monitor-results 2>/dev/null || true # remove output file name
