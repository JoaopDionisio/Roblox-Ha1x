/* 
* 				  RACE SIMULATOR
*      			 Operating Systems
*
*	By: Etiandro André 2017290285 and João Pedro Dionísio 2019217030
*
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#define SIZE_BUF 512
#define MAX_TIME 30
#define LOGFILE "output.log"
#define PIPE_NAME "named_pipe"
#define MAX 300 
#define NUM_TEAMS 3
#define DEBUG
#define MAX_LOG_CHAR 1048
#define RACE_MANAGER_MUT "RACE_MANAGER"
#define STATS "STATISTICS"
#define QUIT 1
#define DONE 2
#define WRITE_LOG "ESCRITA_LOG"

typedef struct Data{
	int numcar;
	int time_units_second;
	int lap_distance;
	int lap_count;
	int team_count;
	int breakdown_check_timer;
	int min_pit;
	int max_pit;
	double fuel_capacity;
}Data;

typedef struct Car
{
	char id_team[MAX];
	int car_id;
	int speed;
	int lap_counter;
	int lap_total;
	double consumption;
	double fuel;
	int fiability;
	int issecurity; // verificar se esta no modo seguranca ou nao(normal)
	int isRacing;
	int isEmpty;
	int isInBox;
	int isquit;
	int isDone;
	int requestbox;
	int box_count;
}Car;

typedef struct Statistics
{
	int top_5[5];
	int last_place;
	int totalavarias;
	int totalabastecimento;
	int numcarrospista;
	Car cars[MAX];
	int estadobox[5]; //0-livre 1-ocupada 2- reservada
	int reserved_id[5];
	
}Statistics;

typedef struct mq_avaria{
	
	long mtype;
	int avaria;

}mq_avaria;

typedef struct Upipe_message
{
	int car_state;
	int car_id;
}Upipe_message;


typedef struct Team{
	int n_car;
	char id_team[MAX];
	char p_id[MAX];
	int isEmpty;
	int box_full;

}Team;


Data data;
pthread_cond_t cond_box=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_box=PTHREAD_MUTEX_INITIALIZER;
Statistics* stats;
pid_t teams[NUM_TEAMS];
pid_t race_simulator, malfunction_manager, race_manager, team_manager;
pthread_t pcars[MAX];
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mut1=PTHREAD_MUTEX_INITIALIZER;
sem_t *mutex_statistic;
sem_t *mutex_log_file;
sem_t *sem_race_managing;
int fd_named_pipe;
int mqid;
int id_stat;
int unamedpipe[2];
Car car_struct[MAX];
sigset_t block_sigs;
Team steams[5];
fd_set read_set;

//--FUNCTIONS-----------------
void handl_sigs();
void print_stats(int signum);
void config();
void createPipe();
void raceManager();
void signal_sigint(int signum);
void handl_sigs();
void malfunctionManager();
void teamManager();
void project_output_log();
void log_file_write(char* words);
void *car(void *n);
void creat_shm_statistics();

void destroy_everything(int n)
{
	switch(n)
	{
		case 1:
			log_file_write("Error in shared memory!\n");
			break;
		case 2:
			log_file_write("Error creating processes\n");
			break;
		case 3:
			log_file_write("Error in message queue\n");
			break;
		case 4:
			log_file_write("Error in signal\n");
			break;
		case 5:
			log_file_write("Error in semaphore\n");
			break;
		case 6:
			log_file_write("Error in threads\n");
			break;
		case 7:
			log_file_write("Error in Pipe\n");
			break;
		case 8:
			log_file_write("Error in log file\n");
			break;
		case 9:
			log_file_write("Error in config file\n");
			break;
	}
	/*
	sem_unlink(STATS);
	sem_unlink(WRITE_LOG);		
	sem_unlink(RACE_MANAGER_MUT);

	unlink(INPUT_PIPE);
	sem_close(mutex_statistic);
	sem_close(mutex_log_file);
	sem_close(sem_race_managing);

	close(unamed_pipe[0]);
	close(unamed_pipe[1]);
	close(fd_named_pipe);
	shmdt(stats);
	shmctl(id_stat,IPC_RMID,NULL);
	msgctl(mqid,IPC_RMID,NULL);
	*/

	printf("Execute kill_ipcs.sh to clean all ipcs");
	system("killall -9 test");
	exit(1);
}


void config (void){
	FILE *file_config;
	static int array[0];
	char *token;
	int i = 0;
	file_config = fopen("config.txt", "r");
	if(file_config == NULL){
		destroy_everything(9);
	}

	char buffer[20];
	while(fgets(buffer, 20, file_config)){
		buffer[strcspn(buffer, "\n")] = 0;
		token = strtok(buffer,",");
		array[i] = atoi(token);
		i++;
		token = strtok(NULL,",");
		if(token != NULL){
			array[i] = atoi(token);
			i++;
		}
	}
	fclose(file_config);
	int *ar;
	ar = array;
	
	data.time_units_second = *ar;
	data.lap_distance = *(ar +1);
	data.lap_count = *(ar+2);  //ver depois
	if(*(ar+3)>2){
		data.team_count = *(ar+3);

	}else{
		
		log_file_write("3 EQUIPAS NECESSARIAS, ALTERE O FICHEIRO CONFIG\n");
		//log_file_write("Minimum of 3 teams required to start race\nExiting simulator...\n");
		exit(1);
	}

	data.numcar = *(ar+4);
	data.breakdown_check_timer = *(ar+5);
	data.min_pit = *(ar+6);
	data.max_pit = *(ar+7);
	data.fuel_capacity = (double)*(ar+8);
}




int main(){ // Race Simulator

	printf("[%d]RACE SIMULATOR\n",getpid());

	
	config();
	handl_sigs();
	/*
	if(signal(SIGINT,SIG_IGN)==SIG_ERR)
	{
		printf("Error: Signal failed!\n");
		
		exit(EXIT_FAILURE);
	}
	if(signal(SIGUSR1,SIG_IGN)==SIG_ERR)
	{
		printf("Error: Signal failed!\n");
		
		exit(EXIT_FAILURE);
	}

	
	if(sem_unlink(RACE_MANAGER_MUT) == EACCES)
		destroy_everything(1);

	if(unlink(INPUT_PIPE) == EACCES)
		destroy_everything(7);
		*/
	if(sem_unlink(WRITE_LOG) == EACCES)
		destroy_everything(5);

	
   	if ((mutex_log_file = sem_open(WRITE_LOG, O_CREAT | O_EXCL, 0700, 1)) == SEM_FAILED)
   		destroy_everything(5);
	
	project_output_log();
	log_file_write("teste\n");
	if(signal(SIGINT,SIG_IGN)==SIG_ERR)
	{
		log_file_write("Error: Signal failed!\n");
		
		exit(EXIT_FAILURE);
	}

	if(signal(SIGINT,signal_sigint)==SIG_ERR)
		destroy_everything(4);


	if(signal(SIGTSTP,SIG_IGN)==SIG_ERR)
    {
        log_file_write("Error: Signal failed!\n");
        exit(EXIT_FAILURE);
    }
    if(signal(SIGTSTP,print_stats)==SIG_ERR)
        destroy_everything(4);

	if(sem_unlink(STATS) == EACCES)
		destroy_everything(5);

	if(sem_unlink(RACE_MANAGER_MUT) == EACCES)
		destroy_everything(1);
	if(unlink(PIPE_NAME) == EACCES)
		destroy_everything(7);

	if ((mutex_statistic = sem_open(STATS, O_CREAT | O_EXCL, 0700, 1)) == SEM_FAILED)
   		destroy_everything(5);
   	if ((sem_race_managing = sem_open(RACE_MANAGER_MUT, O_CREAT | O_EXCL, 0700, 1)) == SEM_FAILED){
	
		destroy_everything(5);
	}

	

   	mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777);
  	if (mqid < 0)
    	destroy_everything(3);
    #ifdef DEBUG
	
	log_file_write("Message Queue created\n");
	#endif
	/*

	log_file_write("Message Queue created\n");
	*/
	
	creat_shm_statistics();
	#ifdef DEBUG
	//log_file_write("Shared memory for statistics  created\n");
	#endif
	
	
	race_simulator = getpid();
	int p_race_manager,p_malfunction_manager;
	
	createPipe();
	
	p_race_manager = fork();
	if (p_race_manager < 0){
		destroy_everything(2);
	}
	else if(p_race_manager == 0)
	{
		raceManager();
		exit(0);
	}
	
	p_malfunction_manager = fork();
	if (p_malfunction_manager < 0){

		destroy_everything(2);
	}
	else if(p_malfunction_manager == 0)
	{
		malfunctionManager();
		exit(0);
	}

	
	
	
	//signal_sigint();
	while(1){}
	
	return 0;
}

void createPipe(){
	pipe(unamedpipe);
	if((mkfifo(PIPE_NAME,O_CREAT|O_EXCL|0600)<0) && (errno !=EEXIST)){
		printf("%d\n", mkfifo(PIPE_NAME,O_CREAT|O_EXCL|0600));
		destroy_everything(7);
	}else{
		//log_file_write("NAMED PIPE CREATED");
	}

	if ((fd_named_pipe = open(PIPE_NAME, O_RDWR)) < 0) {

        		destroy_everything(7);
    	}
    	else{
			
			log_file_write("NAMED PIPE OPENED\n");
			//log_file_write("Named pipe open\n");
		}
	
}


void raceManager()
{
	
	race_manager = getpid();
	char delim[] = " ";
	char command[MAX][MAX];
	Upipe_message up_msg;
	#ifdef DEBUG
	printf("[%d] Team Manager Process created \n",getpid());

	#endif
	//log_file_write("Race Manager Process created\n");
	/*
	for(int i = 0; i< data.team_count; i++){
      		pipe(unamedpipe[i]);
    	}
    	*/
	int dup_id;
	int flag_exceed = 0;
	int team_exceed_index;
	int start = 0;
	int nequipas = 0;
	char line[SIZE_BUF];
	int n;
	close(unamedpipe[1]);
	while(1){
		int bool = 0;
		FD_ZERO(&read_set);
		FD_SET(fd_named_pipe,&read_set);
		FD_SET(unamedpipe[0],&read_set);
		if(select(fd_named_pipe +1, &read_set, NULL, NULL, NULL) > 0){
			
			if(FD_ISSET(fd_named_pipe,&read_set)){                                           
				n = read(fd_named_pipe, &line, sizeof(line));
				if(start == 0){ 	
					line[n-1] = '\0';
					if(!strcmp(line,"START RACE")){
						start = 1;
						
						log_file_write("RACE STARTED\n");
						for (int i = 0; i < data.team_count; i++){
							if((teams[i] = fork()) == 0){
								
								//log_file_write("TEAM MANAGER STARTED\n");
								teamManager(steams[i].id_team);
								exit(0);

							}
							else if(teams[i] == -1){
								destroy_everything(2);
							}
						}
						continue;
					}
					#ifdef DEBUG
					char buff[MAX_LOG_CHAR];
					sprintf(buff,"READ_PIPE: [ %s ]\n",line);
					log_file_write(buff);
					#endif
					char copy[SIZE_BUF];
					strcpy(copy,line);	
					char *ptr = strtok(line, delim);
					
				
					if (strcmp(ptr,"ADDCAR") == 0){
					
						//Teste
						int i = 0;
						while(ptr != NULL)
						{
							strcpy(command[i], ptr);
							ptr = strtok(NULL, delim);
							i++;
						}
						if(i != 6){
							char str1[MAX_LOG_CHAR];
							sprintf(str1,"WRONG COMMAND:[%s]\n", copy);
							
							log_file_write(str1);
							continue;
						}
						
						for(int j = 0; j < stats->numcarrospista; j++){
							
							if(stats->cars[j].car_id == atoi(command[2])){
								
								log_file_write("ID DO CARRO JÀ UTILIZADO\n");
								dup_id = 1;
								break;
							}
						}
						if(dup_id){
							dup_id = 0;
							continue;
						}
						for(int i = 0; i < data.team_count; i++){
							if(strcmp(steams[i].id_team, command[1]) == 0){
								bool = 1;
								if(steams[i].n_car == data.numcar){
									team_exceed_index = i;
									flag_exceed = 1;
									break;
								}
								
								steams[i].n_car += 1;
								
								log_file_write("EQUIPA JA EXISTENTE\n");
								break;
							}
						}
						if(bool == 0){
							nequipas++;
							for(int u = 0; u < data.team_count; u++){
								
								if(steams[u].isEmpty != 1){
									steams[u].n_car = 1;

									strcpy(steams[u].id_team, command[1]);
									
									log_file_write("EQUIPA CRIADA\n");
									steams[u].isEmpty = 1;
									break;	
								}
							}
						
						
						}
						if(nequipas > data.team_count){
							log_file_write("NUMERO DE EQUIPAS EXCEDIDO\n");
							nequipas--;
							continue;
						}
						if(flag_exceed){
							char buff[MAX_LOG_CHAR];
							sprintf(buff,"NUMERO DE CARROS PARA A EQUIPA %d CHEGOU AO LIMITE\n", team_exceed_index);
							log_file_write(buff);
							flag_exceed = 0;
							continue;
						}
						

						for(int u = 0; u < sizeof(stats->cars)/sizeof(stats->cars[0]); u++){
										if(stats->cars[u].isEmpty != 1){
											sem_wait(mutex_statistic);
											stats->numcarrospista += 1;
											sem_post(mutex_statistic);
											strcpy(stats->cars[u].id_team, command[1]);
											stats->cars[u].car_id = atoi(command[2]);	
											stats->cars[u].speed = atoi(command[3]);
											sscanf(command[4], "%lf", &stats->cars[u].consumption);
												
											stats->cars[u].fiability = atoi(command[5]);
											stats->cars[u].isEmpty = 1;
											char buff[MAX_LOG_CHAR];
											sprintf(buff,"NOVO CARRO ADICIONADO => TEAM: %s, CAR: %d, SPEED: %d, CONSUMPTION: %lf, RELIABILITY: %d", stats->cars[u].id_team, stats->cars[u].car_id, stats->cars[u].speed, stats->cars[u].consumption, stats->cars[u].fiability);
											log_file_write(buff);
											break;
									}
								
						}
					}else{
						char str1[MAX_LOG_CHAR];
						sprintf(str1,"COMANDO ERRADO:[%s]\n", copy);
						log_file_write(str1);
					}	
						
				}else{
					log_file_write("A CORRIDA JA FOI INICIADA!\n");
				}

			}
			if(FD_ISSET(unamedpipe[0],&read_set)){
				read(unamedpipe[0],&up_msg, sizeof(Upipe_message));
				
			}

		}

	}
	
}


void teamManager(char* thisid)
{
	
	char buf[512];
	int fd_named_pipe, n;
	//CRIAR THREADS CARRO
	team_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Team Manager Process created\n",getpid());
	#endif
	//Adiciona os carros de acordo aos comandos da named pipe!
	
	char team;
	int values [4];
	char aux[512];
	int first = 1;
	int aux2 = 0;
	int y = 0;
	int b = 1;
	int x = 0;
	
	
	sem_wait(sem_race_managing);
	for (int i = 0; i < MAX; ++i)
	{
		
		if(stats->cars[i].isEmpty != 1){
			
			break;
		}
		if(!strcmp(stats->cars[i].id_team, thisid)){
			
			stats->cars[i].fuel = data.fuel_capacity;
			stats->cars[i].lap_counter=0;
			stats->cars[i].lap_total=0;
			stats->cars[i].issecurity= 0;
			stats->cars[i].isRacing = 1;
			stats->cars[i].isInBox= 0;
			stats->cars[i].requestbox = 0;
			//stats->cars[i].isEmpty= 0;
			stats->cars[i].isDone= 0;
			stats->cars[i].box_count = 0;
			if (pthread_create(&pcars[i], NULL, car, &stats->cars[i].car_id)!=0){
				destroy_everything(6);
			}else{
				log_file_write("Thread 'Car' criada com sucesso!\n");
			}
			
		}
	}
	
	sem_post(sem_race_managing);
	
	
	while(1){
		for(int i = 0; i < stats->numcarrospista; ++i){
			if(stats->cars[i].isInBox && !strcmp(stats->cars[i].id_team, thisid)){
				sem_wait(mutex_statistic);
				stats->cars[i].box_count += 1;
				sem_post(mutex_statistic);
				int randnum = (rand() %(data.max_pit - data.min_pit + 1)) + data.min_pit;
				sleep(randnum);
				sleep(2);
				stats->cars[i].fuel = data.fuel_capacity;
				//semaforo aqui
				sem_wait(mutex_statistic);
				stats->totalabastecimento++;
				sem_post(mutex_statistic);
				if(pthread_mutex_lock(& mutex_box)!=0)
            				destroy_everything(5);

				//alter  condition
				stats->cars[i].isInBox = 0;

				// Wakeup at least one of the threads that are waiting on the condition (if any)
				pthread_cond_signal (&cond_box);

				// allow others to proceed
				if(pthread_mutex_unlock(&mutex_box)!=0) destroy_everything(5);
			}
		}
	}
	
	#ifdef DEBUG
	log_file_write("TODOS OS CARROS FORAM CRIADOS\n");
	#endif
	//sem_post(sem_race_managing);
	exit(0); //Remove later
}

void *car(void* n)
{
	mq_avaria   msg_avaria;
	int id = *((int*)n);
	int indx = 0;
	int teamindx;
	int winner=0;
	Upipe_message up_msg;
	close(unamedpipe[0]);
	for (int i = 0; i < MAX; i++){
		if(stats->cars[i].car_id == id){
			indx = i;
			break;
		}
	}
	for (int i = 0; i < data.team_count; i++){
		if(!strcmp(stats->cars[indx].id_team, steams[i].id_team)){
			teamindx = i;
			break;
		}
	}

	int o = 0;
	srand(id);
	#ifdef DEBUG
	printf("CARRO[%d] SE ENCONTRA NO INICIO DA PISTA\n", id);
	#endif
	
	for(int i = 0; i< data.lap_count; i++){
		if (stats->cars[indx].isquit == 1){
			break;
		}
		o = 0;
		
		if((stats->cars[indx].issecurity == 1 || stats->cars[indx].requestbox == 1) && ((stats->estadobox[teamindx] == 2 && stats->reserved_id[teamindx] == stats->cars[indx].car_id)||stats->estadobox[teamindx] == 0)){
			steams[teamindx].box_full = 1;
			stats->estadobox[teamindx] = 1;
			char buff[MAX_LOG_CHAR];
			sprintf(buff,"O CARRO [%d] ENTROU NA BOX\n", id);
			log_file_write(buff);
			stats->cars[indx].isRacing = 0;
			stats->cars[indx].isInBox = 1;
			if(pthread_mutex_lock(& mutex_box)!=0)
            			destroy_everything(5);
        		while(stats->cars[indx].isInBox)
        		{
            			if (pthread_cond_wait(&cond_box,&mutex_box)!=0){
                 			destroy_everything(5);
        			}
        			if(pthread_mutex_unlock(&mutex_box)!=0)destroy_everything(5);
			}
			
			//setmemoriapartilhadaboxischeia;
			stats->cars[indx].issecurity = 0;
			stats->cars[indx].consumption /= 0.4;
			stats->cars[indx].speed /= 0.3;
			stats->cars[indx].isRacing = 1;
			stats->cars[indx].requestbox = 0;
			stats->estadobox[teamindx] = 0;
			steams[teamindx].box_full = 0;
		}
		stats->cars[indx].lap_counter++;
		while(o < data.lap_distance){
			
			
			if(msgrcv(mqid, &msg_avaria, sizeof(mq_avaria)-sizeof(long), indx+1, IPC_NOWAIT|MSG_NOERROR) == -1){ 
				

			}else{
				
				char buff[MAX_LOG_CHAR];
				sprintf(buff,"AVARIA NO CARRO %d\n", stats->cars[indx].car_id);
				log_file_write(buff);
				stats->cars[indx].issecurity = 1;
				stats->cars[indx].consumption *= 0.4;
				stats->cars[indx].speed *= 0.3;
				stats->cars[indx].requestbox = 1;
				if(stats->estadobox[teamindx] == 0){
					stats->estadobox[teamindx] = 2;
					char buff[MAX_LOG_CHAR];
					sprintf(buff, "BOX DA EQUIPA %s RESERVADA AO CARRO %d\n", steams[teamindx].id_team, stats->cars[indx].car_id);
					log_file_write(buff);
					stats->reserved_id[teamindx] = stats->cars[indx].car_id;
				}
			}
			

			if (stats->cars[indx].fuel < stats->cars[indx].consumption){
				stats->cars[indx].isquit = 1;
				break;
			}else if(stats->cars[indx].fuel <= ((data.lap_distance/stats->cars[indx].speed) * stats->cars[indx].consumption) * 4){
				stats->cars[indx].requestbox = 1;
				if(stats->estadobox[teamindx] == 0){
					stats->estadobox[teamindx] = 2;
					char buff[MAX_LOG_CHAR];
					sprintf(buff, "BOX DA EQUIPA %s RESERVADA AO CARRO %d\n", steams[teamindx].id_team, stats->cars[indx].car_id);
					log_file_write(buff);
					stats->reserved_id[teamindx] = stats->cars[indx].car_id;
				}
				if(stats->cars[indx].fuel <= ((data.lap_distance/stats->cars[indx].speed) * stats->cars[indx].consumption) * 2){
					stats->cars[indx].issecurity = 1;
					//setmemoriapartilhadacarrosecurity;
				}
			}
			sleep(data.time_units_second);
			o += stats->cars[indx].speed;
			stats->cars[indx].fuel -= stats->cars[indx].consumption;
		}
		char aux[MAX_LOG_CHAR];
		sprintf(aux, "O CARRO %d COMPLETOU UMA CORRIDA \n", stats->cars[indx].car_id);
		log_file_write(aux);
		stats->cars[indx].lap_total += 1;

	}
	
	if (stats->cars[indx].isquit == 1){
		char aux[MAX_LOG_CHAR];
		sprintf(aux,"O CARRO %d DESISTIU \n", stats->cars[indx].car_id);
		log_file_write(aux);
		up_msg.car_id = stats->cars[indx].car_id;
		up_msg.car_state = QUIT;

		
		
		write(unamedpipe[1], &up_msg, sizeof(Upipe_message));
		
	}else{
		for(int i = 0; i<5; i++){
			if(stats->top_5[i] == NULL){
				sem_wait(mutex_statistic);
				stats->top_5[i] = stats->cars[indx].car_id; 
				sem_post(mutex_statistic); 
				break;
			}
		}
		stats->cars[indx].isDone = 1;
		winner ++;
		stats->cars[indx].isRacing = 0;
		char buff[MAX_LOG_CHAR];
		sprintf(buff,"CARRO %d TERMINOU A CORRIDA! \n", stats->cars[indx].car_id );
		log_file_write(buff);
		if(winner == 1){
			char aux[MAX_LOG_CHAR];
			sprintf(buff,"CARRO %d VENCEU A CORRIDA! \n", stats->top_5[0] );
			log_file_write(buff);
			winner++;
		}
		
	}
	
	pthread_exit(NULL);

	
	
}


void creat_shm_statistics()
{

	//CHANGE STATS CONFIGURATION LATER
	id_stat = shmget(IPC_PRIVATE,sizeof(Statistics),IPC_CREAT|0777);
	if(id_stat <0){
		
		destroy_everything(1);
		}

	stats = malloc(sizeof(Statistics) + data.team_count * data.numcar * sizeof(int) + data.team_count*sizeof(int));
	stats = (Statistics*)shmat(id_stat,NULL,0);
	if(stats==(Statistics*)-1){
		destroy_everything(1);
		}
	stats->numcarrospista = 0;
	stats->last_place = 0;
	stats->totalavarias = 0;
	stats->totalabastecimento = 0;

	fflush(stdout);
}

void malfunctionManager()
{

	malfunction_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Malfunction Manager Process created\n",getpid());
	if(signal(SIGTSTP,SIG_IGN)==SIG_ERR)
    {
        log_file_write("Error: Signal failed!\n");
        exit(EXIT_FAILURE);
    }
	while(1){
		sleep(data.breakdown_check_timer);
		mq_avaria avr;
		for(int o = 0; o < stats->numcarrospista; o++){
			long mtype = o + 1;
			if(stats->cars[o].isRacing == 1 && stats->cars[o].issecurity == 0 && stats->cars[o].requestbox == 0){
				int randnum = (rand() %(100 - 1 + 1)) + 1;
			
				if (randnum > stats->cars[o].fiability){
					avr.avaria = 1;
					
					avr.mtype = mtype;
					sem_wait(mutex_statistic);                              //trocar semaforos depois
					stats->totalavarias+=1;
					sem_post(mutex_statistic);
					
					if (msgsnd(mqid, &avr, sizeof(mq_avaria)-sizeof(long), 0) == -1){ 
						
           					destroy_everything(3);
           					exit(0);
					}
				}
			}
		}
	}

	#endif
	exit(0); // Remove later
}


void log_file_write(char* words)
{
	/*We call this function everytime we want to write something to log
	 we just need to write the string as an argument
	 semaphore to have write sequence so 2 process cant write at the same time and overwrite it
	*/
	
    if(sem_wait(mutex_log_file)==-1)
		destroy_everything(5);
		
	FILE *log;
	char acttime[MAX_TIME];
	struct tm *get_time;
	time_t moment = time(0);
    get_time = gmtime (&moment);
	log = fopen(LOGFILE,"a");
	if(log==NULL)
		destroy_everything(8);
    strftime (acttime, sizeof(acttime), "%H:%M:%S ", get_time);
	fprintf(log, "%s%s\n",acttime,words);
	fclose(log);
	printf("%s\n", words);
	
	if(sem_post(mutex_log_file)==-1)
		destroy_everything(5);
		
}

void project_output_log()
{
	//Function that opens logfile for write-only option
	FILE *log;
	log = fopen(LOGFILE,"w");
	if(log==NULL)
		destroy_everything(8);
	fclose(log);

}


int search_car_indx_by_id(int id){
	int res = -1;
	for (int i = 0; i < data.numcar*data.team_count; i++){
		if(stats->cars[i].car_id == id){
			res = i;
			break;
		}
	}
	return res;
}
void print_stats(int signum){
    sem_wait(mutex_statistic);
    log_file_write("SINAL SIGTSTP RECEBIDO!\n");
    int indx = 0;
   
    //semaforo para a stats para wait
    printf("-------------TOP 5-----------\n");
    for(int i = 4;  i >= 0; i--){
        indx = search_car_indx_by_id(stats->top_5[i]);
        if(indx == -1)
        	continue;
        printf("%d - ID DO CARRO: %d; \n",i+1, stats->top_5[i]);
        printf("Voltas Completas: %d\n", stats->cars[indx].lap_total);
        printf("EQUIPA: %s\n",stats->cars[indx].id_team);
        printf("Voltas Realizadas: %d\n", stats->cars[indx].lap_counter);
        printf("NUMERO DE PARAGENS NA BOX: %d\n", stats->cars[indx].box_count);
    }
    printf("---------------------------------------\n");

    printf("TOTAL DE AVARIAS DURANTE A CORRIDA: %d\n", stats->totalavarias);
  
    printf("TOTAL DE ABASTECIMENTOS REALIZADOS DURANTE A CORRIDA: %d\n", stats->totalabastecimento);
 
    signal(SIGTSTP, print_stats);
    sem_post(mutex_statistic);
}

void handl_sigs()
{
	sigemptyset (&block_sigs);
	sigaddset (&block_sigs, SIGHUP);
	sigaddset (&block_sigs, SIGQUIT);
	sigaddset (&block_sigs, SIGILL);
	sigaddset (&block_sigs, SIGTRAP);
	sigaddset (&block_sigs, SIGABRT);
	sigaddset (&block_sigs, SIGFPE);
	sigaddset (&block_sigs, SIGBUS);
	sigaddset (&block_sigs, SIGSEGV);
	sigaddset (&block_sigs, SIGSYS);
	sigaddset (&block_sigs, SIGPIPE);
	sigaddset (&block_sigs, SIGALRM);
	sigaddset (&block_sigs, SIGURG);
	//sigaddset (&block_sigs, SIGTSTP);
	sigaddset (&block_sigs, SIGCONT);
	sigaddset (&block_sigs, SIGCHLD);
	sigaddset (&block_sigs, SIGTTIN);
	sigaddset (&block_sigs, SIGTTOU);
	sigaddset (&block_sigs, SIGIO);
	sigaddset (&block_sigs, SIGXCPU);
	sigaddset (&block_sigs, SIGXFSZ);
	sigaddset (&block_sigs, SIGVTALRM);
	sigaddset (&block_sigs, SIGPROF);
	sigaddset (&block_sigs, SIGWINCH);
	sigaddset (&block_sigs, SIGUSR2);
	sigprocmask(SIG_BLOCK,&block_sigs, NULL);
}


void signal_sigint(int signum)
{
	print_stats(1);
	log_file_write("TERMINAÇÃO CONTROLADA A INICIAR...\n");
	//falta thread carros
	if (getpid() == race_simulator)
	{
		
		int i = 0;
		int threads = 0;
		
		
		kill(malfunction_manager,SIGKILL);
		kill(race_manager,SIGKILL);
		while (threads < (stats->numcarrospista) && stats->cars[threads].isDone)
		pthread_join(pcars[threads++], NULL);
		kill(team_manager,SIGKILL);
		while (i < (data.team_count))
		kill(teams[i++], SIGKILL);
		while(wait(NULL) != -1);

		
		if(sem_unlink(STATS) == -1)
			destroy_everything(5);
		if(sem_unlink(LOGFILE) == -1)
			destroy_everything(5);
		if(sem_unlink(RACE_MANAGER_MUT) == -1)
			destroy_everything(5);
		if(unlink(PIPE_NAME) == -1)
			destroy_everything(7);

		if(sem_close(mutex_log_file)==-1)
			destroy_everything(5);

		if(sem_close(mutex_statistic)==-1)
			destroy_everything(5);

		if(sem_close(sem_race_managing)==-1)
			destroy_everything(5);
		
		if(close(fd_named_pipe)==-1)
			destroy_everything(7);

	


		if(shmdt(stats)==-1)
			destroy_everything(1);
		if(shmctl(id_stat,IPC_RMID,NULL)==-1)
			destroy_everything(1);
		if(msgctl(mqid,IPC_RMID,NULL) == -1)
			destroy_everything(3);
		char buff[MAX_LOG_CHAR];
		sprintf(buff,"RACE SIMULATOR[%d] ENDED\n",getpid());
		log_file_write(buff);
		exit(0);
	}

	exit(0);
}

      
