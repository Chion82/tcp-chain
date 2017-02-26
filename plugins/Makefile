CC=gcc -g -std=gnu99

build:
	${CC} -fPIC -c plugin_direct.c
	${CC} -shared plugin_direct.o -o 99-plugin_direct.so

	${CC} -fPIC -c logger.c
	${CC} -shared logger.o -o 98-logger.so
