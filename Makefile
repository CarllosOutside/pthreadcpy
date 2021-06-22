#Makefile for threaded copy 
#Tyler Simon 2/1/2011

CC=cc
#CP=g++34
CP=g++ #variavel CP

all: cores mmio_t t_copy #o make executa esses 3 targets

cores: cores.c #target core depende do arquivo core.c
	 $(CP) -c cores.c  # g++ -c cores.c cria o objeto core.o

mmio_t: mmio_t.c cores.o #target que cria o objeto mmio_t.o dependente de cores.o e mmio_t.c
	gcc -c mmio_t.c #podemos definir -DDEBUG=2,1 -DMAP=1,0 

t_copy: mmio_t.o #target cria o executavel t_copy, usando a dependencia mmio_t.o
	$(CP) -o t_copy mmio_t.o cores.o -lpthread #g++ -o t_copy mmio_t cores.o -lpthread linka
	#os objetos e a biblioteca com as funcionalidades de threads

clean: #make clean para remover os arquivos objeto e executavel
	rm cores.o mmio_t.o t_copy 
