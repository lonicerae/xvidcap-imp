SUBDIRS = xvidcap \
	man

EXTRA_DIST = xmldocs.make \
	omf.make

install-doc-recursive:

@MK@ifneq ($(RECURSIVE_TARGETS),install-doc-recursive)
install-doc-recursive:
	$(MAKE) $(AM_MAKEFLAGS) RECURSIVE_TARGETS=install-doc-recursive install-doc-recursive
@MK@endif

install-doc: install-doc-recursive

install-doc-am:

uninstall-doc-recursive:

@MK@ifneq ($(RECURSIVE_TARGETS),uninstall-doc-recursive)
uninstall-doc-recursive:
	$(MAKE) $(AM_MAKEFLAGS) RECURSIVE_TARGETS=uninstall-doc-recursive uninstall-doc-recursive
@MK@endif

uninstall-doc: uninstall-doc-recursive

uninstall-doc-am:
