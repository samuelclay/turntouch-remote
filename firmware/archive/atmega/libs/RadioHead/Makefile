# Makefile
#
# Makefile for the RadioHead project
#
# Author: Mike McCauley (mikem@airspayce.com)
# Copyright (C) 2014 Mike McCauley
# $Id: Makefile,v 1.22 2014/08/31 01:52:00 mikem Exp mikem $

PROJNAME = RadioHead
VERSION_MAJOR = 1
VERSION_MINOR = 37

DISTFILE = $(PROJNAME)-$(VERSION_MAJOR).$(VERSION_MINOR).zip

all:	versioning doxygen dist upload

# Update version numbers in RadioHead.h
versioning:
	sed -i.bak -e 's/RadioHead-.*\.zip/$(DISTFILE)/' RadioHead.h
	sed -i.bak -e 's/define RH_VERSION_MAJOR.*$$/define RH_VERSION_MAJOR $(VERSION_MAJOR)/' RadioHead.h
	sed -i.bak -e 's/define RH_VERSION_MINOR.*$$/define RH_VERSION_MINOR $(VERSION_MINOR)/' RadioHead.h

doxygen: 
	doxygen project.cfg

ci:
	(cd ..;ci -l `cat $(PROJNAME)/MANIFEST`)

dist:	
	(cd ..; zip $(PROJNAME)/$(DISTFILE) `cat $(PROJNAME)/MANIFEST`)

upload:
	rsync -avz $(DISTFILE) doc/ www.airspayce.com:public_html/mikem/arduino/$(PROJNAME)
