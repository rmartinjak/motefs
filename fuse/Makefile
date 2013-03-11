include ../Makefile.include

#CFLAGS += -DDEBUG
CFLAGS += -g
CFLAGS += -Wall -Wextra

CFLAGS += -I/usr/include/fuse
CFLAGS += -D_FILE_OFFSET_BITS=64

CFLAGS += -Dnx_uint8_t=uint8_t
CFLAGS += -I../
CFLAGS += -Itos/

CFLAGS += -DMFS_DATA_SIZE=$(MFS_DATA_SIZE)

LIBS += tos/libmote.a
LIBS += -lfuse -pthread

SRCDIR = src
OBJDIR = obj

TARGETS = motefs test

_OBJ = mfsmsg serial
OBJ = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(_OBJ)))


MOTEFS_H = ../nesc/MoteFS/MoteFS.h
MFSMSG_STRUCT = motefs_msg
MFSMSG_PREFIX = mfsmsg
MFSCONST =	MFS_RDONLY \
			MFS_WRONLY \
			MFS_RDWR \
			MFS_BOOL \
			MFS_INT \
			MFS_STR \
			MFS_OP_READ \
			MFS_OP_WRITE \
			MFS_OP_NODELIST \
			MFS_OP_NODECOUNT \
			MFSMSG_BUFSZ


default : motefs

$(OBJDIR) :
	mkdir $(OBJDIR)

$(SRCDIR)/mfsmsg.c : $(SRCDIR)/mfsmsg.h

$(SRCDIR)/mfsmsg.h : $(MOTEFS_H)
	mig c -target=null -c-prefix=$(MFSMSG_PREFIX) -DMFS_DATA_SIZE=$(MFS_DATA_SIZE) -o $@ $< $(MFSMSG_STRUCT)

$(SRCDIR)/mfsconst.h : $(MOTEFS_H)
	ncg c -target=null $< $(MFSCONST) -DMFS_DATA_SIZE=$(MFS_DATA_SIZE) -o $@

$(OBJDIR)/%.o : $(SRCDIR)/%.c $(SRCDIR)/mfsconst.h | $(OBJDIR)
	@echo CC -c $<
	@$(CC) $(CFLAGS) -c -o $@ $<

$(TARGETS) : % : $(SRCDIR)/%.c $(OBJ)
	@echo CC -o $@
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)


clean :
	rm -f $(SRCDIR)/mfsmsg.c $(SRCDIR)/mfsmsg.h
	rm -f $(SRCDIR)/mfsconst.h
	rm -rf $(OBJDIR)
	rm -f $(TARGETS)
