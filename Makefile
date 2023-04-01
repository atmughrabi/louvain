CC=gcc
CFLAGS=-O3 -g
EXEC=louvain

all: $(EXEC)
	./louvain graph.el graph.out  

louvain: partition.o louvain.o
	$(CC) -o louvain partition.o louvain.o $(CFLAGS)

clean:
	rm *.o louvain

%.o: %.c %.h
	$(CC) -o $@ -c $< $(CFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

run:
	./louvain graph.el graph.out    