## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/lib

noinst_HEADERS = red_black_tree.h stack.h
noinst_LTLIBRARIES = librbtree_C.la

librbtree_C_la_SOURCES = misc.c red_black_tree.c stack.c

EXTRA_DIST = LICENSE CHANGES makefile.txt test_red_black_tree.c \
	rbtree.vcxproj*
