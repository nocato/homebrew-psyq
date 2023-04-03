all: psyq-4.4-old-school

psyq-4.4-old-school:
	./build-scripts/run-debian-slink-build-in-docker.sh

psyq-4.4-old-school-package:
	./build-scripts/package-old-school.sh

psyq-4.4-old-school-test:
	./build-scripts/test-old-school.sh

.PHONY: all clean psyq-4.4-old-school psyq-4.4-old-school-package

clean:
	rm -rf build
	rm -f gcc-2.8.1_psyq-4.4/gcc/bi-parser.c
	rm -f gcc-2.8.1_psyq-4.4/gcc/bi-parser.h
	rm -f gcc-2.8.1_psyq-4.4/gcc/c-parse.c
	rm -f gcc-2.8.1_psyq-4.4/gcc/c-parse.h
	rm -f gcc-2.8.1_psyq-4.4/gcc/c-parse.y
	rm -f gcc-2.8.1_psyq-4.4/gcc/config.in
	rm -f gcc-2.8.1_psyq-4.4/gcc/configure
