bin_PROGRAMS = DumpGDSIIFile GDSII2GMSH

DumpGDSIIFile_SOURCES = DumpGDSIIFile.cc
DumpGDSIIFile_LDADD = $(top_builddir)/lib/libGDSII.la

GDSII2GMSH_SOURCES = GDSII2GMSH.cc
GDSII2GMSH_LDADD   = $(top_builddir)/lib/libGDSII.la

AM_CPPFLAGS = -I$(top_srcdir)/lib
