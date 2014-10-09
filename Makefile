GPP      = /opt/rh/devtoolset-2/root/usr/bin/g++
GPPFLAGS = -g -O0 -Wall -Wextra -std=gnu++11
OBJECTS=main.o stringset.o

all : stringset

stringset : ${OBJECTS}
	${GPP} ${GPPFLAGS} main.o stringset.o -o stringset

%.o : %.cpp
	${GPP} ${GPPFLAGS} -c $<

spotless : clean
	-rm stringset

clean :
	-rm stringset.o main.o

