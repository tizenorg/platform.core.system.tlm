SUBDIRS =. default

all-local: createlibs

createlibs:
	$(MKDIR_P) .libs

if HAVE_LIBGUM
    SUBDIRS += gumd
endif

