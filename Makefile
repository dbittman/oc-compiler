# $Id: Makefile,v 1.8 2014-10-07 18:13:45-07 - - $

GPP        = g++ -g -O0 -Wall -Wextra -std=gnu++11
MKDEP      = ${GPP} -MM -std=gnu++11
VALGRIND   = valgrind --leak-check=full --show-reachable=yes

MKFILE     = Makefile
DEPFILE    = Makefile.dep
SOURCES    = cppstrtok.cpp main.cpp stringset.cpp astree.cpp lyutils.cpp auxlib.cpp
GENSRCS    = yyparse.cpp yylex.cpp
HEADERS    = stringset.h oc.h auxlib.h lyutils.h astree.h
OBJECTS    = ${SOURCES:.cpp=.o} ${GENSRCS:.cpp=.o}
EXECBIN    = oc
SRCFILES   = ${HEADERS} ${SOURCES} ${MKFILE}
SMALLFILES = ${DEPFILE} foo.oc foo1.oh foo2.oh
SUBMITS    = ${SRCFILES} README parser.y scanner.l

all : ${EXECBIN}

${EXECBIN} : ${OBJECTS}
	${GPP} -o${EXECBIN} ${OBJECTS}

%.o : %.cpp
	${GPP} -c $<

yyparse.h yyparse.cpp : parser.y
	bison parser.y -o yyparse.cpp --defines=yyparse.h

yylex.cpp : scanner.l yyparse.h
	flex -o yylex.cpp scanner.l

ci :
	cid + ${SUBMITS}
	checksource ${SUBMITS}

clean :
	- rm ${OBJECTS} ${GENSRCS} yyparse.h yyparse.output

spotless : clean
	- rm ${EXECBIN} ${LISTING} ${LISTING:.ps=.pdf} ${DEPFILE} \
	     test.out test.err misc.lis

${DEPFILE} : ${SOURCES} ${GENSRCS}
	${MKDEP} ${SOURCES} ${GENSRCS} >${DEPFILE}

dep :
	- rm ${DEPFILE}
	${MAKE} --no-print-directory ${DEPFILE}

include Makefile.dep

test : ${EXECBIN}
	${VALGRIND} ./${EXECBIN} foo.oc 1>test.out 2>test.err

checks:
	/afs/cats.ucsc.edu/courses/cmps104a-wm/bin/checksource ${SOURCES}

submit:
	submit cmps104a-wm.f14 asg2 ${SUBMITS}
	mkdir -p sub
	cp ${SUBMITS} sub/

