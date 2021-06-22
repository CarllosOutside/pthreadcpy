/**************************************
 * Tyler Simon 
 * mmmio_t.c 
 * memory mapped I/O with threads 
 * put a file in memory and copy 
 * to another file concurrently with pthreads 
 *
 * Don't use this on anything you don't have backed up, this code does not guarantee
 * data integrity. This is a research prototype use at your own risk, that
 * being said if you have any problems please let me know. 
 * 
 *
 * 8/5/09: Original
 * 4/04/10 Added automatic core detection
 * 10/14/10 Added thread<->core affinity  via core.c
 * 
 *
 *************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>


#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef MAP
#define MAP 1
#endif

#ifndef HALF
#define HALF 0
#endif

#ifndef SYNC
#define SYNC 0
#endif

#define MAX_THREADS 255

double getCurrentTime(void);
void *tcopy_segment(void*);


struct thread_data{ /*uma estrutura para cada thread*/
    int t_id; /*numero do thread*/
    int t_fd; /*fd do arquivo fonte*/
    int t_fdout; /*fd do destino*/
    long t_buf_size; /*o tamanho do pedaço dos dados do arquivo fonte que cada thread
	será responsável por copiar*/
};
struct thread_data thread_data_array[MAX_THREADS];

char *buf;
char *data; /*memória do processo mapeada no arquivo fonte*/
char *out;
float residual;
ssize_t totbytes=0; /*guarda o total de bytes escritos em um arquivo*/
int cpufreq, nprocs;







int getNumCPU();
void getCPU(int cpu);

int main(int argc, char *argv[])
{
int fd, fdout,i,rc,t;/*descriptor do arquivo fonte, destino*/
double t0,t1,t2,t3;
struct stat sbuf; /*extrair informações de arquivo*/
char fname[255]; /*nome do arquivo fonte*/
char fnameout[255]; /*nome do arquivo destino*/
int nthreads,tid;
pthread_attr_t tattr;
size_t stacksize=1048576; /*pilha de 1MB*/
long int bread;
char hname[255];/*nome do host*/
double elapsedTime; /*tempo de copia*/
double start_time,end_time; /*cronometro*/

if (argc != 3) { /*e args devem ser passados, nome do programa arquivo a ser copiado e 
arquivo que recebera a copia*/
fprintf(stderr, "usage: %s <input file> <output file>\n", argv[0]);/*argv[0] é o nome do programa*/
exit(1);
}

/*ARGUMENTOS PRA EXECUÇÃO*/
strcpy(fname,argv[1]); /*fname éo nome arquivo fonte que sera copiado*/
strcpy(fnameout,argv[2]); /*fnameout o nome do arquivo que recebera a copia*/

gethostname(hname,255); /*guarda o nome do host na variavel hname*/

/*CORES HABILITADOS*/
/* see how many processors we have*/
if(HALF)nprocs=getNumCPU()/2; /*verifica qtd de cores que estao habilitados para executar o
processo atual e divide por 2-half*/
else nprocs=getNumCPU(); /*verifica qtd de cores habilitados no processo atual*/

printf("Numero de cores ativos %d\n", nprocs);
/*INÍCIO DE EXECUÇÃO*/
//get processinfo
printf("Iniciando contagem de tempo\n");
start_time=getCurrentTime(); /*inicia cronometro*/


/*ABRINDO ARQUIVO FONTE*/
printf("Abrindo arquivo fonte\n");
fd=open(fname,O_RDONLY); /*abre o arquivo fonte agora referenciado por fd*/

/*SE DER ERRO NA ABERTURA*/
if(fd<0){/*se der erro retorna -1*/
if(errno)/*errno guarda o numero do ultimo erro do sistema*/
    {/*We know there is an error*/

    printf("Error %d: %s %s\n", errno, fname,strerror(errno)); /*descreve o erro*/
    exit(1);
    }
else
perror("problems opening file");
exit(1);
}


if(!fchdir(fd)){/*verifica se o fd especifica um diretório ao inves do arquivo fonte */
    puts("is a directory");
    exit(1);
}

if(access(fname, F_OK)) /*checa a existência do arquivo fonte, deve retornar 0*/
    {
    printf("Error %d: %s %s\n", errno, fname,strerror(errno));
    exit(1); 
    }

/*se o arquivo fonte e o destino dados pelo usuario forem iguais, termina*/
if (strcmp(fname,fnameout)==0)exit(1);


/*ABRINDO ARQUIVO DESTINO*/
printf("Abrindo arquivo destino\n");
unlink(fnameout); /**/

if (fdout=open(fnameout,O_WRONLY,O_NONBLOCK)<0) /*abre o arquivo destino agora referen
ciado por fdout*/
    {
     if(errno == ENOENT) /*se o arquivo nao existir*/
           {
           if ((fdout = creat(fnameout, O_RDWR)) < 0) /*cria o arquivo e abre*/
            { perror("creat error");
                exit(1);
            }
           }else
           {perror("please specify destination file");
            exit(1);
           }
           }
chmod(fnameout,0600); /*syscall permite que o usuário leia e escreva no arquivo 
fnameout*/



if (stat(fname, &sbuf) == -1) { /*extrai informações da fonte na estrutura sbuf*/
perror("stat");
exit(1);
}
printf("Tamanho do arquivo fonte %ld \n", sbuf.st_size);
residual=sbuf.st_size%nprocs; /*bytes que sobram da divisao por nucleos*/
printf("Cada processo ficara responsavel por copiar %ld bytes, e o resto é de %f\n", sbuf.st_size/nprocs, residual);
pthread_t threads[nprocs]; /*Cria nprocs id's para novos threads*/


/*MAPEANDO O ARQUIVO FONTE*/
data=malloc(sbuf.st_size); /*alocando buffer para mapear arquivo fonte*/
if(!data)
    {
    perror("malloc Failed:");
    exit(1);
    }
/*					Argsmmap:0 = segmento de memoria escolhido pelo sistema
							sbu.st_size = tamanho dos dados no arquivo fonte a serem mapeados
							PROT_READ = leitura protegida
							MAP_SHARED = todamodificação em data será diretamente aplicada no arquivo fd
							fd = arquivo fonte
							0= offset de onde começamos a mapea-lo(arquivo)	*/
if ((data = mmap((caddr_t)0, sbuf.st_size, PROT_READ, MAP_SHARED,fd, 0)) == (caddr_t)(-1)) {
perror("mmap"); /*mapeia o arquivo fonte, de tamanho st_size, na memória do processo que
será referenciada como data. Toda leitura/escrita em data afetará o arquivo fonte*/
exit(1);
}
printf("Arquivo fonte mapeado para o segemento %p de memoria do processo\n", data);

close(fd);/*podemos fechar o arquivo e fazer as alterações em data*/
printf("%s: Copying %s --> %s with %d threads.\n",hname, fname,fnameout,nprocs);

/*criando nprocs(numero de cores disponiveis) threads*/
for (t=0;t<nprocs; t++)
    {
    thread_data_array[t].t_id = t;
    thread_data_array[t].t_fd = fd;
    thread_data_array[t].t_fdout = fdout;
    if(t==nprocs-1)
    thread_data_array[t].t_buf_size = (sbuf.st_size/nprocs)+residual; /*Um thread 
	fica responsavel pelo resto, além de seu pedaço inteiro(o len desse trhread sera maior que dos outros)*/
    else
    thread_data_array[t].t_buf_size = sbuf.st_size/nprocs;
    		/*bytes dos dados a serem copiados distribuidos entre os threads*/

//    thread_data_array[t].t_page_size = residual;


    /* setting the size of the stack also */
    pthread_attr_init(&tattr);/*inicializando atributo*/
    pthread_attr_setstacksize(&tattr, stacksize); /*uma pilha de 1MB para as variaveis do thread*/

    rc=pthread_create(&threads[t],&tattr, tcopy_segment, (void*) &thread_data_array[t]);
    /*cria um thread, id armazenado em threads[t], atributos em tattr(stacksize)
	chama a função tcopy_segment, com os 4 parametros da estrutura thread_data_array */
	if (rc){/*verifica se foi criado  com exito*/
     printf("ERROR; return code from pthread_create() is %d\n", rc);
     perror("Error creating Thread");
     }
  //system("ps -eo pmem,rss,psr,policy,nlwp,\%cpu,etime,ruser,cmd --sort uid");

pthread_join(threads[t], NULL);/*o thread pai(MAIN) aguarda o filho*/
    }
//pthread_exit(NULL);
if (SYNC)sync();  /*syscall para limpar os buffers se SYNC for definido*/
close(fdout); /*após término de cópia, fecha o arquivo destino*/
end_time=getCurrentTime(); /*finaliza cronometro*/
printf("Finalizando cronometro, calculando tempo de execução\n");
elapsedTime = (double) (end_time-start_time); /*tempo total de execucao em seg*/

printf("\nCopy Complete in %2.1f sec. %2.1f (MB/s)\n", elapsedTime,(sbuf.st_size/1048576)/elapsedTime);
return 0; /*printa o tempo de execucao e a taxa de transferencia       bytes convertido pra MB/ segundos passados*/
}//end main











//void *copy_segment( int from, int to, long int len , off_t page, off_t offset)
void *tcopy_segment(void *thread_args)
{
pthread_detach(pthread_self()); /*thread e seus recursos são limpos da memória sem
a necessidade de join*/
int count=0;

/*EXTRAINDO PARAMETROS*/
struct thread_data *local_data;  /*ponteiro para parametros/dados de thread*/  
local_data = (struct thread_data *)thread_args; /*convertemos void para o tipo thread_data
agora temos acessos aos parametros passados na criação do thread*/
int taskid = local_data->t_id; /*numero do thread*/
int from=local_data->t_fd; /*fd da fonte*/
int to=local_data->t_fdout; /*fd do destino*/
long len=local_data->t_buf_size; /*tamanho do PEDAÇO de dados*/
 printf("Processo %d iniciado, mapeando o core %d\n", taskid, taskid);
/* Map a thread to a core - default*/
    if(MAP)getCPU(taskid); /*são n threads para n cores, o enésimo thread executará no 
	enésimo core*/

long int myshare;/*offset do responsavel pelo resto*/

/*opcional*/
if(DEBUG==2)printf("id=%d to=%d from=%d len=%ld res=%f offset = %f\n", taskid, to, from, len, residual, (len-residual)*taskid);

 printf("Escrevendo no arquivo..\n");
if(taskid==nprocs-1){/*o processo responsavel pelo resto*/
myshare=(len-residual)*taskid; /*esse processo tem len maior=len+residual, 
então para fazer o offset correto, é necessário reduzir o len dele*/
lseek(to, myshare,SEEK_CUR); /*offset no destino*/
totbytes+=write(to,data+myshare,len); /*Escreve no destino, o buff=data com offset myshare(começa daí)
igual ao offset do destino.
,tam=len
se escrever com sucesso, o numero de bytes é 
retornado e armazenado em totbytes*/
if(errno){printf("error writing = %d\n", errno);}
}
else /*lembre que data é um endereço, e soma-lo com len é avançar na memoria(posições)*/
totbytes+=pwrite(to,data+(len*taskid),len,len*taskid); /*1º par: Escreve no arquivo 
destino
4º par: Começa a esrcrever do offset len*taskid no arquivo destino, len é o tamanho do pedaço de cada thread
,que multiplicado pelo numero do processo, aponta para a região que ele é responsavel 
por escrever.
3º par: Escreve len bytes, que é a qtd que o processo foi responsabilizado.
2º par: o processo 1(de taskid=0) escrevera desde o começo do segmento data, 
o processo 2 escrevera do fim desse segmento(data_len) para frente, 
o 3 processo escrevera do fim do segmento do processo 2(data+len*2) pra frente
ou seja, cada processo começa de uma lugar diferente
Apos o processo escrever sua parte, o total de bytes escritos é incrementado
Perceba que o 4ºpar: é o offset no arquivo de saída
O 2ºpar:é o ínicio do segmento de memoria onde começamos a escrever
O Offset é o mesmo para 'data' e 'to' */

/*opcional*/
if(DEBUG==1)
{
    char cpucmd[255];
     sprintf(cpucmd, "ps -p %d -L -o cmd,thcount,pid,tid,pcpu,pmem,time,size,psr", getpid()); 
     system(cpucmd);
}

/*opcional*/
if(DEBUG==2)printf("%d residual = %f blocklength = %ld totbytes = %ld \n",taskid, residual ,len,  totbytes);
//msync(len,totbytes,MS_ASYNC);

 printf("Terminando processo\n");
/*TERMINA*/
pthread_exit(NULL);
}/*end copy segment */










double getCurrentTime(void) /*função que retorna uma representação do tempo atual em segundo*/
{   
    struct timeval tval; /*estrutura timeval guarda o tempo*/
    gettimeofday(&tval, NULL); /*aramzena na estrutura, timezone=NULL*/
    return (tval.tv_sec + tval.tv_usec/1000000.0); /*tempo em segundo
	segundo + segundo^-6*10^6*/
}
