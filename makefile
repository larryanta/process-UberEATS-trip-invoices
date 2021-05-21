all: rpt1pgm rpt2pgm rpt3pgm 
	rm -f rpt1pgm.o
	rm -f rpt2pgm.o
	rm -f rpt3pgm.o

clean:
	rm -f rpt1pgm rpt2pgm rpt3pgm rpt1pgm.o rpt2pgm.o rpt3pgm.o

rpt1pgm: rpt1pgm.o
	gcc -Wall -lz -o rpt1pgm rpt1pgm.c
rpt2pgm: rpt2pgm.o
rpt3pgm: rpt3pgm.o

rpt1pgm.o: rpt1pgm.c
	gcc -Wall -c rpt1pgm.c
rpt2pgm.o: rpt2pgm.c
	gcc -Wall -c rpt2pgm.c
rpt3pgm.o: rpt3pgm.c
	gcc -Wall -c rpt3pgm.c
