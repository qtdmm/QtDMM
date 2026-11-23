ARCHIV = build.tar.gz
SPEC =  $(notdir $(spec))
RPM_ROOT := $(shell rpm --eval %_topdir)

srpm:
	echo RAUS $(outdir) 
	echo SPEC $(spec)
	mkdir -p $(RPM_ROOT)/{SOURCES,SPECS}
	tar czf $(RPM_ROOT)/SOURCES/$(ARCHIV) .
	cp $(spec) $(RPM_ROOT)/SPECS/
	sed -i "s|^Source0.*|Source0:\t$(ARCHIV)|g" $(RPM_ROOT)/SPECS/$(SPEC)
	# the archive carries the checkout's .git: with git in the build root,
	# cmake/git_version.cmake gives the build its version (else 0.0-unknown)
	sed -i "0,/^BuildRequires:/s|^BuildRequires:|BuildRequires:\tgit-core\n&|" $(RPM_ROOT)/SPECS/$(SPEC)
	rpmbuild -bs $(RPM_ROOT)/SPECS/$(SPEC)
	mv $(RPM_ROOT)/SRPMS/* $(outdir)/
