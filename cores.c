#include <sys/resource.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

extern "C"{
/* get total cores on system*/
int getNumCPU() {
cpu_set_t mask;/*grupo de bits onde cada bit mapeia uma cpu do sistema*/
if (sched_getaffinity(0,sizeof(cpu_set_t),&mask)) { 
/*verifica os cores que estao habilitados par o processo atual e os insere em mask, deve
retornar zero*/
fprintf (stderr, "Unable to retrieve affinity\n");
exit(1);
}
int nproc = 0,i; /*nproce = cores ativados*/
for(i=0; i<CPU_SETSIZE; i++) /*Para o máximo de cpus possíveis*/ 
   {
    if( CPU_ISSET(i,&mask) ) { /*verifica se cada cpu esta ativa*/
    nproc++; /*para cada cpu ativa, incrementa o numero de cores habilitados nesse processo*/
    }
   }
return nproc; /*retorna o numero total de cores*/
}

void getCPU(int cpu) /*o core desejado é passado como parametro*/
    {
    cpu_set_t mask;/*máscara de cores*/
    CPU_ZERO(&mask); /*limpando a máscara*/
    CPU_SET(cpu, &mask); /*inserindo o core desejado*/
    sched_setaffinity(0, sizeof(cpu_set_t), &mask); /*o processo que chamou a função,
	rodará no cpu especificado como parâmetro e setado na máscara
		processo atual=0
		tamanho da mascara
		mascara(contendo o core desejado)*/
    }
}//end extern
