CC=gcc -g -std=gnu99
RMF=rm -rf

default:
	make clean
	make build

build:
	make build_main
	make build_plugins

build_main:
	${CC} -c main.c
	${CC} main.o -o aegis -lev -ldl

build_plugins:
	cd plugins && make build

clean:
	${RMF} *.o
	${RMF} aegis
	${RMF} plugins/*.o
	${RMF} plugins/*.so
	cd lib/* && make clean

