# Makefile for hev-scgi-handler-cgi
 
PROJECT=hev-scgi-handler-cgi

CROSS_PREFIX :=
PP=$(CROSS_PREFIX)cpp
CC=$(CROSS_PREFIX)gcc
AR=$(CROSS_PREFIX)ar
STRIP=$(CROSS_PREFIX)strip
PKG_DEPS=glib-2.0 gio-2.0
CCFLAGS=-O3 -Wall -Werror -I../hev-scgi-server-library/include \
	`pkg-config --cflags $(PKG_DEPS)`
LDFLAGS=
 
SRCDIR=src
BINDIR=bin
BUILDDIR=build

STATIC_TARGET=$(BINDIR)/libhev-scgi-handler-cgi.a
SHARED_TARGET=$(BINDIR)/libhev-scgi-handler-cgi.so

$(STATIC_TARGET): CCFLAGS+=-DSTATIC_MODULE
$(SHARED_TARGET): CCFLAGS+=-fPIC
$(SHARED_TARGET): LDFLAGS+=-shared \
	-L../hev-scgi-server-library/bin -lhev-scgi-server \
	`pkg-config --libs $(PKG_DEPS)` 

CCOBJS = $(wildcard $(SRCDIR)/*.c)
LDOBJS = $(patsubst $(SRCDIR)%.c,$(BUILDDIR)%.o,$(CCOBJS))
DEPEND = $(LDOBJS:.o=.dep)

BUILDMSG="\e[1;31mBUILD\e[0m $<"
LINKMSG="\e[1;34mLINK\e[0m  \e[1;32m$@\e[0m"
STRIPMSG="\e[1;34mSTRIP\e[0m \e[1;32m$@\e[0m"
CLEANMSG="\e[1;34mCLEAN\e[0m $(PROJECT)"

V :=
ECHO_PREFIX := @
ifeq ($(V),1)
	undefine ECHO_PREFIX
endif

shared : $(SHARED_TARGET)

static : $(STATIC_TARGET)
 
clean : 
	$(ECHO_PREFIX) $(RM) $(BINDIR)/* $(BUILDDIR)/*
	@echo -e $(CLEANMSG)
 
$(STATIC_TARGET) : $(LDOBJS)
	$(ECHO_PREFIX) $(AR) cqs $@ $^
	@echo -e $(LINKMSG)

$(SHARED_TARGET) : $(LDOBJS)
	$(ECHO_PREFIX) $(CC) -o $@ $^ $(LDFLAGS)
	@echo -e $(LINKMSG)
	$(ECHO_PREFIX) $(STRIP) $@
	@echo -e $(STRIPMSG)
 
$(BUILDDIR)/%.dep : $(SRCDIR)/%.c
	$(ECHO_PREFIX) $(PP) $(CCFLAGS) -MM -MT $(@:.dep=.o) -o $@ $<
 
$(BUILDDIR)/%.o : $(SRCDIR)/%.c
	$(ECHO_PREFIX) $(CC) $(CCFLAGS) -c -o $@ $<
	@echo -e $(BUILDMSG)
 
-include $(DEPEND)

