all: clcg4.h clcg4.c assignment5.c
	gcc -I. -Wall -O3 -c clcg4.c -o clcg4.o
	mpicc -I. -Wall -O3 assignment5.c clcg4.o -o assignment5 -lpthread 
