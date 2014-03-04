
-include $(top_srcdir)/git.mk

man_MANS =
if ENABLE_MAN
man_MANS += appdata-validate.1
endif

m4datadir = $(datadir)/aclocal
dist_m4data_DATA = \
	appdata-xml.m4

xmlschemadir = $(datadir)/appdata/schema
dist_xmlschema_DATA = 				\
	appdata.xsd				\
	schema-locating-rules.xml		\
	appdata.rnc

# Some validators (e.g., jing) can't handle Schematron embedded in RELAX
# NG, so extract it for use separately:
if ENABLE_SCHEMAS
%.rng: %.rnc
	$(TRANG) $< $@
%.sch: %.rng
	$(PYTHON) -c 'import sys; from lxml import isoschematron, etree; print isoschematron.extract_rng(etree.parse(sys.argv[-1]).getroot())' $< > $@
dist_xmlschema_DATA += appdata.rng appdata.sch
endif

XSLTPROC_FLAGS = \
	--nonet \
	--stringparam man.output.quietly 1 \
	--stringparam funcsynopsis.style ansi \
	--stringparam man.th.extra1.suppress 1 \
	--stringparam man.authors.section.enabled 0 \
	--stringparam man.copyright.section.enabled 0

appdata-validate.1: appdata-validate.xml
	$(AM_V_GEN) xsltproc $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

edit = sed -e 's|@datadir[@]|$(datadir)|g'

appdata-rng-init.el:
	rm -f $@ $@.tmp
	srcdir=''; \
	    test -f ./$@.in || srcdir=$(srcdir)/; \
	    $(edit) $${srcdir}$@.in >$@.tmp
	mv $@.tmp $@

appdata-rng-init.el: appdata-rng-init.el.in

emacsinitdir = $(datadir)/emacs/site-lisp/site-start.d
emacsinit_DATA = appdata-rng-init.el

EXTRA_DIST =					\
	appdata-rng-init.el.in			\
	appdata-validate.xml			\
	broken.appdata.xml			\
	metadata.appdata.xml			\
	screenshots.appdata.xml			\
	success.appdata.xml			\
	short-para-before-list.appdata.xml	\
	translated.appdata.xml			\
	wrong-extension.xml

clean-local:
	rm -f *~
	rm -f *.1
	rm -f manpage.*
	rm -f *.el
	rm -f *.rng
	rm -f *.sch

CLEANFILES =						\
	*.rng						\
	*.sch						\
	appdata-rng-init.el				\
	appdata-validate.1				\
	manpage.*
