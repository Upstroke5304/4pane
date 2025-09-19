
all-local: clean-local
	@if ! test -f $(MSGFMT) ; then \
		echo "msgfmt not found. You probably need to install the gettext package"; \
		exit 1; \
	else \
## The complexity is because of 'make distcheck' which builds in a subdir, so we have to mkdir before we can msgfmt. $(srcdir) is ../../locale and $(builddir) '.'
## The 'find' output is e.g. ./fr.po which we cut to get 'fr'
		find $(srcdir) -path *LC_MESSAGES/*.po -execdir sh -c 'echo {} | cut -d/ -f2 | cut -d. -f1' \; | xargs -I {} -- sh -c '$(MKDIR_P) $(abs_builddir)/{}/LC_MESSAGES && $(MSGFMT) $(srcdir)/{}/LC_MESSAGES/{}.po -o $(abs_builddir)/{}/LC_MESSAGES/4Pane.mo' ; \
	fi

distclean-local: clean-local

clean-local:
	@`find $(builddir) -path *LC_MESSAGES/*.mo -execdir rm -f {} \;`
