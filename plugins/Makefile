CC=gcc -g -std=gnu99

build:

	list=$(shell ls -l | grep ^d)
	@echo $(list)

	${CC} -fPIC -c plugin_direct.c
	${CC} -shared plugin_direct.o -o 99-plugin_direct.so

	${CC} -fPIC -c logger.c
	${CC} -shared logger.o -o 98-logger.so

	${CC} -fPIC -c waf.c
	${CC} -shared waf.o -o 70-waf.so

	cd ../lib/hiredis/ && make
	${CC} -fPIC -c anti_brute_force.c
	${CC} -shared anti_brute_force.o ../lib/hiredis/libhiredis.a -o 50-anti_brute_force.so

	${CC} -fPIC -c anti_crawler.c -lssl -lcrypto
	${CC} -shared anti_crawler.o -o 80-anti_crawler.so -lssl -lcrypto

