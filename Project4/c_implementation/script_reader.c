/*Angelis Marios,Kasidakis Theodoros*/
/*AEM:2406,2258*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include "group_list.h"
#include "load_balancing_list.h"
#define LOAD_BALANCING_TIME 100
#define GAP 2
#define ALARM_TIME 2

void set_array(char table[MAX_ROWS][MAX_SIZE]);
char my_ip_address[MAX_IP_SIZE];
int my_port,first_run=0,shut_down_flag=0;
struct program_node *running_program,*temp_running_program;
struct group_node * running_group,*temp_running_group;
void *my_runtime();
void *my_receiver();
void *balancing_sender();
void *multicast_receiver();
int split_command(char *input,char array_of_args[MAX_ROWS][MAX_SIZE]);
int find_members(char array_of_args[MAX_ROWS][MAX_SIZE],int length);
int signal_bit=0,instruction_bit=0,program_id=0,load_balancing_counter=0,load_balancing_flag=0;
double prev_time=0,start_time,group_id=0.0;
sem_t mtx,balancing_mtx,str_tok_mtx;
struct timespec tv1_start_time;
struct timespec tv2_current_time;
struct timespec unique;
struct sigaction act_alarm={{0}};
	
void alarm_handler(int sig){
	
	int running_program_deleted=0,running_group_deleted=0;
	if(instruction_bit==1){
		signal_bit=1;
		return;
	}
	if(signal_bit==1){signal_bit=0;}
	clock_gettime( CLOCK_PROCESS_CPUTIME_ID ,&tv2_current_time);
	double current_time=(double) (tv2_current_time.tv_nsec - tv1_start_time.tv_nsec) / 1000000000.0 + (double) (tv2_current_time.tv_sec - tv1_start_time.tv_sec);
	
	update_sleep_time(prev_time,current_time);
	prev_time=current_time;
	
	running_program_deleted=check_for_delete_programs(running_program->program_id);
	running_group_deleted=check_for_delete_groups(running_group->group_id);
	
	if(running_group_deleted==1){
		running_program_deleted=1;
	}
	if(running_group_deleted==0 && running_program_deleted==0){
		running_program=get_next_program(running_program,&running_group);
	}
	else{
		running_program=get_next_program_from_start(&running_group);
	}
	if(running_program->group_id!=1) {
		//printf("Change to program [%lf.%d]\n",running_program->group_id,running_program->program_id);
	}
	alarm(ALARM_TIME);
	if(load_balancing_flag==0){
		load_balancing_counter++;
		//printf("load balancing counter=%d\n",load_balancing_counter);
		if(load_balancing_counter==LOAD_BALANCING_TIME){
			load_balancing_flag=1;
			sem_post(&balancing_mtx);
		}
	}
}
int main(int argc,char *argv[]) {
	
	char array_of_args[MAX_ROWS][MAX_SIZE];
	char command[MAX_INPUT_SIZE],temp_command[MAX_INPUT_SIZE],str[MAX_INPUT_SIZE];
	struct program_node *program;
	struct sockaddr_in other_runtime;
	struct migrate_node migrate_struct;
	struct program_node *current_program;
	struct group_node *group;
	pthread_t thread,thread2,thread3,thread4;
	int i,error=0,exiting=0,ret=0,iret,j=0,valid_command=0,iret2,sock,already_cpy=0,iret3,iret4,receiver;

	//printf("%ld\n",sizeof(struct migrate_node));
	//printf("%ld\n",sizeof(struct second_step));
    //printf("%ld\n",sizeof(struct third_step));
	sem_init(&mtx,0,0);
	sem_init(&balancing_mtx,0,0);
	sem_init(&str_tok_mtx,0,1);
	
	strcpy(my_ip_address,argv[1]);
	my_port=my_atoi(argv[2]);
	
	group_list_init();
	balance_list_init();
	act_alarm.sa_handler=alarm_handler;
	act_alarm.sa_flags=SA_RESTART;
	sigaction(SIGALRM,&act_alarm,NULL);
	clock_gettime( CLOCK_PROCESS_CPUTIME_ID , &tv1_start_time);
	
	program_id++;
	group_id++;
	group=group_list_add(group_id,1);
	
	program=program_list_add(group,program_id,group_id,"idle.txt");
	program->fd=fopen("idle.txt","r");
	if(program->fd==NULL) {
		printf("Error with fopen().\n");
		exit(1);
	}
	program_id=0;
	iret = pthread_create(&thread,NULL,my_runtime,NULL);
	if(iret){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
		exit(EXIT_FAILURE);
	}
	iret2 = pthread_create(&thread2,NULL,my_receiver,NULL);
	if(iret2){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
		exit(EXIT_FAILURE);
	}
	iret3 = pthread_create(&thread3,NULL,balancing_sender,NULL);
	if(iret3){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret3);
		exit(EXIT_FAILURE);
	}
	iret4 = pthread_create(&thread4,NULL,multicast_receiver,NULL);
	if(iret4){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret4);
		exit(EXIT_FAILURE);
	}
    sem_wait(&mtx);
	
	//Read running_program->commands with fgets
	while(1){
		sem_wait(&str_tok_mtx);
		instruction_bit=1;
		if(running_program->label_found==0){
			bzero(command,strlen(command));
			while(1){
				//If command is an empty line with spaces or tabs,read the next command
				fgets(command,MAX_INPUT_SIZE,running_program->fd);
				//printf("len =%ld,command is %s",strlen(command),command);
				for(i=0;i<strlen(command);i++){
					if(isalpha(command[i])!=0){
						valid_command=1;
						break;
					}
				}
				if(valid_command==1){
					valid_command=0;
					break;
				}
			}
			strcpy(running_program->command,command);
			running_program->command[strlen(running_program->command)-1]='\0';
			command[strlen(command)-1]='\0';
			if(command[0]=='	'){
				running_program->token=strtok(command,"	");
				running_program->token=strtok(running_program->token," ");
			}
			else{
				running_program->token=strtok(command," ");
			}
		}
		else{
			running_program->label_found=0;
			bzero(temp_command,MAX_INPUT_SIZE);
			strcpy(temp_command,running_program->command);
			running_program->token=strtok(temp_command," ");
			running_program->token=strtok(NULL," ");
		}
		//Add function
		if((strcmp(running_program->token,"	ADD")==0) || (strcmp(running_program->token,"ADD")==0)){
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//ADD sum $sum1 3 is an invalid syntax running_program->command,but ADD $sum $sum 0 is a valid running_program->command
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				strcpy(array_of_args[i],running_program->token);
				i++;
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call add_function to save array_of_args[1]+array_of_args[2] to array_of_args[0]
				running_program->variable_list_head=add_function(running_program->variable_list_head,"NULL",array_of_args[0],array_of_args[1],array_of_args[2],&error);
				//Reset the array_of_args
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	SUB")==0) || (strcmp(running_program->token,"SUB")==0)){
			//Sub function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//SUB sum $sum1 3 is an invalid syntax running_program->command,but SUB $sum $sum 0 is a valid running_program->command
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				strcpy(array_of_args[i],running_program->token);
				i++;
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call sub_function to save array_of_args[1]-array_of_args[2] to array_of_args[0]
				running_program->variable_list_head=sub_function(running_program->variable_list_head,"NULL",array_of_args[0],array_of_args[1],array_of_args[2],&error);
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	MUL")==0) || (strcmp(running_program->token,"MUL")==0)){
			//Mul function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//MUL sum $sum1 3 is an invalid syntax running_program->command,but MUL $sum $sum 0 is a valid running_program->command
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				strcpy(array_of_args[i],running_program->token);
				i++;
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call mul_function to save array_of_args[1]*array_of_args[2] to array_of_args[0]
				running_program->variable_list_head=mul_function(running_program->variable_list_head,"NULL",array_of_args[0],array_of_args[1],array_of_args[2],&error);
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	DIV")==0) || (strcmp(running_program->token,"DIV")==0)){
			//Div function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//DIV sum $sum1 3 is an invalid syntax running_program->command but also DIV $sum $sum 0 is an invalid running_program->command
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				strcpy(array_of_args[i],running_program->token);
				i++;
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call div_function to save array_of_args[1]/array_of_args[2] to array_of_args[0]
				running_program->variable_list_head=div_function(running_program->variable_list_head,"NULL",array_of_args[0],array_of_args[1],array_of_args[2],&error);
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	MOD")==0) || (strcmp(running_program->token,"MOD")==0)){
			//Mod function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//MOD sum $sum1 3 is an invalid syntax running_program->command but also MOD $sum $sum 0 is an invalid running_program->command
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				strcpy(array_of_args[i],running_program->token);
				i++;
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call mod_function to save array_of_args[1]%array_of_args[2] to array_of_args[0]
				running_program->variable_list_head=mod_function(running_program->variable_list_head,"NULL",array_of_args[0],array_of_args[1],array_of_args[2],&error);
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	SLP")==0) || (strcmp(running_program->token,"SLP")==0)){
			//Sleep function
			running_program->token=strtok(NULL," ");
			//SLP is an invalid running_program->command but also SLP -10 is an invalid running_program->command
			if(running_program->token[0]!='$' && my_atoi(running_program->token)<=0){
				printf("Invalid syntax\n");
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call slp_function to sleep "running_program->token" seconds
				running_program->sleep_time=slp_function(running_program->variable_list_head,"NULL",running_program->token,&error);
				running_program->sleep_bit=1;
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
				else{
					instruction_bit=0;
					//printf("sending sigalrm to handler\n");
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	PRN")==0) || (strcmp(running_program->token,"PRN")==0)){
			//Print function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//Split and save all the strings between ""
				if(running_program->token[0]=='"'){
					if(running_program->token[strlen(running_program->token)-1]=='"'){
 						running_program->token[strlen(running_program->token)-1]='\0';
						strcpy(array_of_args[i],&running_program->token[1]);
						i++;
						already_cpy=1;
					}
					else{
						strcpy(array_of_args[i],&running_program->token[1]);
						i++;
						while(1){
							running_program->token=strtok(NULL," ");
							if(running_program->token[strlen(running_program->token)-1]=='"'){
								running_program->token[strlen(running_program->token)-1]='\0';
								break;
							}
							strcpy(array_of_args[i],running_program->token);
							i++;
						}
					}
				}
				else if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				//Maybe the last world was " ,so we converted this string to \0.Also,maybe the last word was cda",so we converted it to cda\0
				if(running_program->token[0]=='\0'){}
				else if(already_cpy==0){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				if(already_cpy==1){already_cpy=0;}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call print_function to print all the array_of_args strings
				running_program->variable_list_head=print_function(running_program->variable_list_head,array_of_args,&error,running_program->program_id,running_program->group_id);
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	SET")==0) || (strcmp(running_program->token,"SET")==0)){
			//Set function
			i=0,j=0;
			int counter=0,exists_ret=1;
			char temp[MAX_INPUT_SIZE];
			char string_value[MAX_INPUT_SIZE];
			int integer_value;
			running_program->token=strtok(NULL," ");
			
			if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
				printf("Invalid syntax\n");
				exiting=1;
			}
			else{
				strcpy(array_of_args[i],running_program->token);
				i++;
				running_program->token=strtok(NULL," ");
				
				//SET $sum 5 or SET $sum $sum2
				if(running_program->token[0]!='"'){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else{
					//SET $sum "string"
					while(j<strlen(running_program->command)){
						//Copy the string.String maybe has spaces
						if(running_program->command[j]=='"'){
							temp[counter]=running_program->command[j];
							counter++;
							j++;
							while(1){
								if(running_program->command[j]=='"'){break;}
							
								temp[counter]=running_program->command[j];
								j++;
								counter++;
							}
							temp[counter]=running_program->command[j];
							temp[counter+1]='\0';
							strcpy(array_of_args[i],temp);
							bzero(temp,MAX_INPUT_SIZE);
							counter=0;
							i++;
						}
						j++;
					}
				}
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call set_function to initialize variable array_of_args[0] to array_of_args[1](SET $sum $sum1 || SET $sum $sum2 || SET $sum "string")
				//SET $sum "string"
				if(array_of_args[1][0]=='"'){
					running_program->variable_list_head=set_function(running_program->variable_list_head,array_of_args[1],-1,array_of_args[0],&error);
				}
				else if(array_of_args[1][0]=='$'){
					exists_ret=check_var_exists(running_program->variable_list_head,array_of_args[1],string_value,&integer_value);
				
					if(exists_ret==-1){
						printf("Unknown variable %s\n",array_of_args[1]);
						error=1;
					}
					else{
						//SET $sum $sum1 and $sum1 is a string
						if(exists_ret==0){running_program->variable_list_head=set_function(running_program->variable_list_head,string_value,-1,array_of_args[0],&error);}
						else{
							//SET $sum $sum1 and $sum1 is an integer
							running_program->variable_list_head=set_function(running_program->variable_list_head,"NULL",integer_value,array_of_args[0],&error);
						}
					}
				}
				else{
					//SET $sum 10
					running_program->variable_list_head=set_function(running_program->variable_list_head,"NULL",my_atoi(array_of_args[1]),array_of_args[0],&error);
				}
				set_array(array_of_args);
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	RET")==0) || (strcmp(running_program->token,"RET")==0)){
			//Program is just finished!
			if(running_group->original==1){
				printf("ret \n");
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				memset(&migrate_struct,0,sizeof(migrate_struct));
				migrate_struct.program_id=htonl(running_program->program_id);
				migrate_struct.group_id=running_program->group_id;
				
				sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				migrate_struct.type=htonl(RET_MESSAGE);
				
				other_runtime.sin_family = AF_INET; 
				inet_aton(running_program->home_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(running_program->home_port); 
				if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
					printf("Connection with the other_runtime failed 1\n"); 
					exit(1); 
				} 
				send(sock,&migrate_struct,sizeof(struct migrate_node),0);
				
				close(sock);
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
		}
		else if((strcmp(running_program->token,"	BEQ")==0) || (strcmp(running_program->token,"BEQ")==0)){
			//Beq function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//Label must begin with #.
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && running_program->token[0]!='#' && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				if(running_program->token[0]!='#'){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else{
					//save the label
					strcpy(running_program->label,running_program->token);
				}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call beq_function to check if array_of_args[0] is equal to array_of_args[1]
				ret=beq_function(running_program->variable_list_head,array_of_args[0],array_of_args[1],&error);
				if(ret==0){
					set_array(array_of_args);
					fseek(running_program->fd,0,SEEK_SET);
					//Search the file to find the same label.Execute the running_program->command in the same line
					while(1){
						while(1){
							bzero(command,strlen(command));
							//If command is an empty line with spaces or tabs,read the next command
							fgets(command,MAX_INPUT_SIZE,running_program->fd);
							//printf("len =%ld,command is %s",strlen(command),command);
							for(i=0;i<strlen(command);i++){
								if(isalpha(command[i])!=0){
									valid_command=1;
									break;
								}
							}
							if(valid_command==1){
								valid_command=0;
								break;
							}
						}
						strcpy(running_program->command,command);
						running_program->command[strlen(running_program->command)-1]='\0';
						command[strlen(command)-1]='\0';
						running_program->token=strtok(command," ");
						if(strcmp(running_program->label,running_program->token)==0){
							running_program->label_found=1;
							break;
						}
					}
				}
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	BGT")==0) || (strcmp(running_program->token,"BGT")==0)){
			//Bgt function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//Label must begin with #.
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && running_program->token[0]!='#' && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				else if(running_program->token[0]!='#'){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else{
					strcpy(running_program->label,running_program->token);
				}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call bgt_function to check if array_of_args[0] is greater than the array_of_args[1]
				ret=bgt_function(running_program->variable_list_head,array_of_args[0],array_of_args[1],&error);
				if(ret==0){
					set_array(array_of_args);
					fseek(running_program->fd,0,SEEK_SET);
					//Search the file to find the same label.Execute the running_program->command in the same line
					while(1){
						while(1){
							bzero(command,strlen(command));
							//If command is an empty line with spaces or tabs,read the next command
							fgets(command,MAX_INPUT_SIZE,running_program->fd);
							//printf("len =%ld,command is %s",strlen(command),command);
							for(i=0;i<strlen(command);i++){
								if(isalpha(command[i])!=0){
									valid_command=1;
									break;
								}
							}
							if(valid_command==1){
								valid_command=0;
								break;
							}
						}
						strcpy(running_program->command,command);
						running_program->command[strlen(running_program->command)-1]='\0';
						command[strlen(command)-1]='\0';
						
						if(command[0]=='	'){
							running_program->token=strtok(command,"	");
							running_program->token=strtok(running_program->token," ");
						}
						else if(command[0]=='#'){
							running_program->token=strtok(command," ");
						}
						if(strcmp(running_program->label,running_program->token)==0){
							running_program->label_found=1;
							break;
						}
					}
				}
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	BGE")==0) || (strcmp(running_program->token,"BGE")==0)){
			//Bge function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//Label must begin with #.
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && running_program->token[0]!='#' && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				else if(running_program->token[0]!='#'){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else{strcpy(running_program->label,running_program->token);}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call bge_function to check if array_of_args[0] is greater than the array_of_args[1] or equal to array_of_args[1]
				ret=bge_function(running_program->variable_list_head,array_of_args[0],array_of_args[1],&error);
				if(ret==0){
					set_array(array_of_args);
					fseek(running_program->fd,0,SEEK_SET);
					//Search the file to find the same label.Execute the running_program->command in the same line
					while(1){
						while(1){
							bzero(command,strlen(command));
							//If command is an empty line with spaces or tabs,read the next command
							fgets(command,MAX_INPUT_SIZE,running_program->fd);
							//printf("len =%ld,command is %s",strlen(command),command);
							for(i=0;i<strlen(command);i++){
								if(isalpha(command[i])!=0){
									valid_command=1;
									break;
								}
							}
							if(valid_command==1){
								valid_command=0;
								break;
							}
						}
						
						strcpy(running_program->command,command);
						running_program->command[strlen(running_program->command)-1]='\0';
						command[strlen(command)-1]='\0';
						running_program->token=strtok(command," ");
						if(strcmp(running_program->label,running_program->token)==0){
							running_program->label_found=1;
							break;
						}
					}
				}
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	BLT")==0) || (strcmp(running_program->token,"BLT")==0)){
			//Blt function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//Label must begin with #.
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && running_program->token[0]!='#' && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				else if(running_program->token[0]!='#'){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else{
					strcpy(running_program->label,running_program->token);
				}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call blt_function to check if array_of_args[0] is lower than the array_of_args[1]
				ret=blt_function(running_program->variable_list_head,array_of_args[0],array_of_args[1],&error);
				if(ret==0){
					set_array(array_of_args);
					fseek(running_program->fd,0,SEEK_SET);
					//Search the file to find the same label.Execute the running_program->command in the same line
					while(1){
						while(1){
							bzero(command,strlen(command));
							//If command is an empty line with spaces or tabs,read the next command
							fgets(command,MAX_INPUT_SIZE,running_program->fd);
							//printf("len =%ld,command is %s",strlen(command),command);
							for(i=0;i<strlen(command);i++){
								if(isalpha(command[i])!=0){
									valid_command=1;
									break;
								}
							}
							if(valid_command==1){
								valid_command=0;
								break;
							}
						}
						strcpy(running_program->command,command);
						running_program->command[strlen(running_program->command)-1]='\0';
						command[strlen(command)-1]='\0';
						running_program->token=strtok(command," ");
						if(strcmp(running_program->label,running_program->token)==0){
							running_program->label_found=1;
							break;
						}
					}
				}
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	BLE")==0) || (strcmp(running_program->token,"BLE")==0)){
			//Ble function
			i=0;
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//Label must begin with #.
				if(running_program->token[0]!='$' && my_atoi(running_program->token)==0 && running_program->token[0]!='#' && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				else if(running_program->token[0]!='#'){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else{
					strcpy(running_program->label,running_program->token);
				}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//Call ble_function to check if array_of_args[0] is lower than the array_of_args[1] or equal to array_of_args[1]
				ret=ble_function(running_program->variable_list_head,array_of_args[0],array_of_args[1],&error);
				if(ret==0){
					set_array(array_of_args);
					fseek(running_program->fd,0,SEEK_SET);
					while(1){
						//Search the file to find the same label.Execute the running_program->command in the same line
						while(1){
							bzero(command,strlen(command));
							//If command is an empty line with spaces or tabs,read the next command
							fgets(command,MAX_INPUT_SIZE,running_program->fd);
							//printf("len =%ld,command is %s",strlen(command),command);
							for(i=0;i<strlen(command);i++){
								if(isalpha(command[i])!=0){
									valid_command=1;
									break;
								}
							}
							if(valid_command==1){
								valid_command=0;
								break;
							}
						}
						
						strcpy(running_program->command,command);
						running_program->command[strlen(running_program->command)-1]='\0';
						command[strlen(command)-1]='\0';
						
						running_program->token=strtok(command," ");
						if(strcmp(running_program->label,running_program->token)==0){
							running_program->label_found=1;
							break;
						}
					}
				}
				if(error==1){
					error=0;
					running_program->delete_bit=1;
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	BRA")==0) || (strcmp(running_program->token,"BRA")==0)){
			//Bra function
			i=0;
			running_program->token=strtok(NULL," ");
			strcpy(running_program->label,running_program->token);

			set_array(array_of_args);
			fseek(running_program->fd,0,SEEK_SET);
			//Search the file to find the same label.Execute the running_program->command in the same line
			while(1){
				while(1){
					bzero(command,strlen(command));
					//If command is an empty line with spaces or tabs,read the next command
					fgets(command,MAX_INPUT_SIZE,running_program->fd);
					//printf("len =%ld,command is %s",strlen(command),command);
					for(i=0;i<strlen(command);i++){
						if(isalpha(command[i])!=0){
							valid_command=1;
							break;
						}
					}
					if(valid_command==1){
						valid_command=0;
						break;
					}
				}
				strcpy(running_program->command,command);
				running_program->command[strlen(running_program->command)-1]='\0';
				command[strlen(command)-1]='\0';
				running_program->token=strtok(command," ");
				if(strcmp(running_program->label,running_program->token)==0){
					running_program->label_found=1;
					break;
				}
			}
		}
		else if((strcmp(running_program->token,"	SND")==0) || (strcmp(running_program->token,"SND")==0)){
			//send function
			i=0;
			j=0;
			//printf("send fuction\n");
			set_array(array_of_args);
			int counter=0,string_ahead=0,blocking=0;
			char temp[MAX_INPUT_SIZE];
			running_program->token=strtok(NULL," ");
			if(running_program->token[0]=='$'){
				check_var_exists(running_program->variable_list_head,running_program->token,"NULL",&receiver);
				
				sprintf(str, "%d",receiver);
				strcpy(array_of_args[i],str);
				//printf("%s\n",array_of_args[i]);
				i++;
				
			}
			else{
				strcpy(array_of_args[i],running_program->token);
				i++;
			}
			//Find all command strings.Copy them to array_of_args
			while(j<strlen(running_program->command)){
				if(running_program->command[j]=='"'){
					temp[counter]=running_program->command[j];
					j++;
					counter++;
					while(1){
						if(running_program->command[j]=='"'){break;}
						temp[counter]=running_program->command[j];
						j++;
						counter++;
					}
					temp[counter]=running_program->command[j];
					temp[counter+1]='\0';
					strcpy(array_of_args[i],temp);
					bzero(temp,MAX_INPUT_SIZE);
					counter=0;
					i++;
					
				}
				j++;
			}
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				//find all variables of the command(SND 10 "string  abcd" $sum1 87)
				if(running_program->token[0]=='"' && string_ahead==0){
					string_ahead=1;
				}
				else if(running_program->token[0]=='$' && string_ahead==0){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else if((running_program->token[strlen(running_program->token)-1]=='"')){
					string_ahead=0;
				}
				else if(running_program->token[0]!='$' && string_ahead==0 && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				else if(string_ahead==0 && ((running_program->token[0]!='$' && my_atoi(running_program->token)!=0) || strcmp(running_program->token,"0")==0)){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//printf("array_of_args[%d]=%s\n",0,array_of_args[0]);
				
				//Call snd_function to send the message
				if(strcmp(running_program->home_ip,my_ip_address)==0 && my_port==running_program->home_port){
					for(current_program=running_group->program_list_head->next;;current_program=current_program->next){
						if(current_program==running_group->program_list_head){
							running_program->send_bit=1;
							instruction_bit=0;
							kill(getpid(),SIGALRM);
							break;
						}
						if(current_program->program_id==my_atoi(array_of_args[0])){
							if(current_program->migrate_bit==1){
								memset(&migrate_struct,0,sizeof(migrate_struct));
								//Receiver is migrated
								migrate_struct.sender_id=htonl(running_program->program_id);
								migrate_struct.receiver_id=htonl(current_program->program_id);
								fix_message_arguments_for_migration_send(running_program,array_of_args,i);
								//for(j=0;j<i;j++){
								//	printf("array_of_args[%d] is %s\n",j,array_of_args[j]);
								//}
								migrate_struct.length=htonl(i);
								migrate_struct.group_id=current_program->group_id;
								for(j=0;j<i;j++){
									strcpy(migrate_struct.array_of_args[j],array_of_args[j]);
								}
								sock = socket(AF_INET, SOCK_STREAM, 0); 
								if (sock == -1) { 
									printf("TCP Socket creation failed\n"); 
									exit(1); 
								}
								migrate_struct.type=htonl(SEND_MESSAGE);
								
								other_runtime.sin_family = AF_INET; 
								inet_aton(current_program->migrate_ip,&other_runtime.sin_addr);
								other_runtime.sin_port = htons(current_program->migrate_port); 
								if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
									printf("Connection with the other_runtime failed 1\n"); 
									exit(1); 
								} 
								send(sock,&migrate_struct,sizeof(struct migrate_node),0);
								running_program->send_bit=1;
								close(sock);
								instruction_bit=0;
								kill(getpid(),SIGALRM);
								break;
							}
							else{
								//printf("send to %d\n",current_program->program_id);
								running_program=snd_function(running_group,running_program,array_of_args,&error,i,&blocking);
								set_array(array_of_args);
								if(blocking==1){
									instruction_bit=0;
									kill(getpid(),SIGALRM);
									break;
								}
								else if(error==1){
									error=0;
									running_program->delete_bit=1;
									instruction_bit=0;
									kill(getpid(),SIGALRM);
									break;
								}
							}
						}
					}
				}
				else{
					memset(&migrate_struct,0,sizeof(migrate_struct));
					//Send snd_message to home runtime 
					migrate_struct.sender_id=htonl(running_program->program_id);
					migrate_struct.receiver_id=htonl(my_atoi(array_of_args[0]));
					fix_message_arguments_for_migration_send(running_program,array_of_args,i);
					migrate_struct.length=htonl(i);
					migrate_struct.group_id=running_program->group_id;
					
					for(j=0;j<i;j++){
						strcpy(migrate_struct.array_of_args[j],array_of_args[j]);
					}
					
					sock = socket(AF_INET, SOCK_STREAM, 0); 
					if (sock == -1) { 
						printf("TCP Socket creation failed\n"); 
						exit(1); 
					} 
					
					migrate_struct.type=htonl(SEND_MESSAGE);
					
					other_runtime.sin_family = AF_INET; 
					inet_aton(running_program->home_ip,&other_runtime.sin_addr);
					other_runtime.sin_port = htons(running_program->home_port); 
					if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed 1\n"); 
						exit(1); 
					}
					send(sock,&migrate_struct,sizeof(struct migrate_node),0);
					set_array(array_of_args);
					running_program->send_bit=1;
					close(sock);
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
			}
		}
		else if((strcmp(running_program->token,"	RCV")==0) || (strcmp(running_program->token,"RCV")==0)){
			//receive function
			i=0;
			j=0;
			set_array(array_of_args);
			int counter=0,string_ahead=0,blocking=0;
			char temp[MAX_INPUT_SIZE];
			running_program->token=strtok(NULL," ");
			
			if(running_program->token[0]=='$'){
				check_var_exists(running_program->variable_list_head,running_program->token,"NULL",&receiver);
				
				sprintf(str, "%d",receiver);
				strcpy(array_of_args[i],str);
				//printf("receive from %s\n",array_of_args[i]);
				i++;
				
			}
			else{
				strcpy(array_of_args[i],running_program->token);
				i++;
			}
			
			
			//Find all command strings.Copy them to array_of_args
			while(j<strlen(running_program->command)){
				if(running_program->command[j]=='"'){
					temp[counter]=running_program->command[j];
					j++;
					counter++;
					while(1){
						if(running_program->command[j]=='"'){break;}
					
						temp[counter]=running_program->command[j];
						j++;
						counter++;
					}
					temp[counter]=running_program->command[j];
					temp[counter+1]='\0';
					strcpy(array_of_args[i],temp);
					bzero(temp,MAX_INPUT_SIZE);
					counter=0;
					i++;
					
				}
				j++;
			}
			//find all variables of the command(RVC 10 "string  abcd" $sum1 87)
			running_program->token=strtok(NULL," ");
			while(running_program->token!=NULL) {
				if(running_program->token[0]=='"' && string_ahead==0){
					string_ahead=1;
				}
				else if(running_program->token[0]=='$' && string_ahead==0){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				else if((running_program->token[strlen(running_program->token)-1]=='"')){
					string_ahead=0;
				}
				else if(running_program->token[0]!='$' && string_ahead==0 && my_atoi(running_program->token)==0 && strcmp(running_program->token,"0")!=0){
					printf("Invalid syntax\n");
					exiting=1;
					break;
				}
				else if(string_ahead==0 && ((running_program->token[0]!='$' && my_atoi(running_program->token)!=0) || strcmp(running_program->token,"0")==0)){
					strcpy(array_of_args[i],running_program->token);
					i++;
				}
				running_program->token=strtok(NULL," ");
			}
			if(exiting==1){
				exiting=0;
				running_program->delete_bit=1;
				instruction_bit=0;
				kill(getpid(),SIGALRM);
			}
			else{
				//for(j=0;j<i;j++){
				//	printf("array_of_args[%d] is %s\n",j,array_of_args[j]);
				//}
				//Call rcv_function to receive a message
				running_program=rcv_function(running_group,running_program,array_of_args,&error,i,&blocking);
				if(blocking==1){
					set_array(array_of_args);
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
				else if(error==1){
					error=0;
					running_program->delete_bit=1;
					set_array(array_of_args);
					instruction_bit=0;
					kill(getpid(),SIGALRM);
				}
				else if(blocking==2){
					memset(&migrate_struct,0,sizeof(migrate_struct));
					//Sender is migrated
					
					migrate_struct.sender_id=htonl(my_atoi(array_of_args[0]));
					migrate_struct.group_id=running_program->group_id;
					sock = socket(AF_INET, SOCK_STREAM, 0); 
					if (sock == -1) { 
						printf("TCP Socket creation failed\n"); 
						exit(1); 
					}
					
					migrate_struct.type=htonl(SENDER_WAKE_UP);
					
					other_runtime.sin_family = AF_INET; 
					inet_aton(running_program->home_ip,&other_runtime.sin_addr);
					other_runtime.sin_port = htons(running_program->home_port); 
					if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed 1\n"); 
						exit(1); 
					} 
					send(sock,&migrate_struct,sizeof(struct migrate_node),0);
					close(sock);
					set_array(array_of_args);
				}
			}
		}
		else if(running_program->token[0]=='#'){
			//Found a label.Continue with the execution of the running_program->command in the same line
			strcpy(running_program->label,running_program->token);
			running_program->label_found=1;
		}
		else{
			printf("Invalid command\n");
			running_program->delete_bit=1;
			instruction_bit=0;
			kill(getpid(),SIGALRM);
		}
		instruction_bit=0;
		
		if(signal_bit==1 && running_program->label_found!=1){
			kill(getpid(),SIGALRM);
		}
		sem_post(&str_tok_mtx);
	}
	return(0);
}
//Reset the 2 dimension table of strings
void set_array(char table[MAX_ROWS][MAX_SIZE]){
	int i=0;
	for(i=0;i<MAX_ROWS;i++){
		bzero(table[i],MAX_SIZE);
	}
}
void *my_runtime(){
	
	struct program_node *program,*kill_program;
	struct group_node *group;
	struct migrate_node *migrate_struct,kill_struct;
	struct second_step *second_step_send;
	struct third_step *third_step_send;
	struct sockaddr_in other_runtime;
	char array_of_args[MAX_ROWS][MAX_SIZE];
	char args_name[MAX_IP_SIZE];
	char str[MAX_IP_SIZE];
	char input[MAX_INPUT_SIZE];
	int i,j=1,create_program=0,kill_group_ret=0,error,args_counter=1,members_of_group=0,sock;
	while(1){
		fgets(input,MAX_INPUT_SIZE,stdin);
		sem_wait(&str_tok_mtx);
		args_counter=1;
		set_array(array_of_args);
		
		input[strlen(input)-1]='\0';
		if(strcmp(input,"list")==0){
			print_group_list();
		}
		else{
			j=1;
			create_program=0;
			i=split_command(input,&array_of_args[0]);
			if(strcmp(array_of_args[0],"run")==0){
				group_id++;
				clock_gettime( CLOCK_PROCESS_CPUTIME_ID ,&unique);
				group_id=(double) (unique.tv_nsec - tv1_start_time.tv_nsec) / 1000000000.0 + (double) (unique.tv_sec - tv1_start_time.tv_sec);
				members_of_group=find_members(&array_of_args[0],i);
				
				group=group_list_add(group_id,members_of_group);
				group->original=1;
				program_id=0; 
				//Reset the counter of the programms when a new group has appeared
				//A new group has created.Calculate how many programs run command has.
				while(j<=i){ 
					if(j==1 || create_program==1){
						printf("create a program with name %s\n",array_of_args[j]);
						create_program=0;
						program_id++;
						program=program_list_add(group,program_id,group_id,array_of_args[j]);
						program->fd=fopen(array_of_args[j],"r");
						if(program->fd==NULL) {
							printf("Error with fopen().\n");
							exit(1);
						}
						strcpy(program->home_ip,my_ip_address);
						program->home_port=my_port;
						program->variable_list_head=set_function(program->variable_list_head,array_of_args[j],0,"$arg0",&error);
					}
					else{ 
						//If it isn't an || symbol or a programm name,then it would be an argument 
						strcpy(args_name,"$arg");
						sprintf(str, "%d",args_counter);
						strcat(args_name,str);
						program->variable_list_head=set_function(program->variable_list_head,"NULL",my_atoi(array_of_args[j]),args_name,&error);
						args_counter++;
					}
					j++;
					if(j>=i){
						program->variable_list_head=set_function(program->variable_list_head,"NULL",args_counter,"$argc",&error);
						break;
                    }
					if(strcmp(array_of_args[j],"||")==0){ //After this symbol we must create another programm to that group
						program->variable_list_head=set_function(program->variable_list_head,"NULL",args_counter,"$argc",&error);
						create_program=1;
						args_counter=1;
						j++; //Move j so we can obtain the name of the next programm and its args (if it has args)
					}
				}
				if(first_run==0){
					running_group=group;
					running_program=program;
					first_run=1;
					alarm(ALARM_TIME);
					sem_post(&mtx);
				}
			}
			else if(strcmp(array_of_args[0],"kill")==0){
				//Kill a group
				printf("Kill group %lf\n",strtod(array_of_args[1],NULL));
				if(compare_float(strtod(array_of_args[1],NULL),strtod("1",NULL))==1){
					printf("Can not kill idle process\n");
				}
				else{
					//kill_group_ret=kill_group(strtod(array_of_args[1],NULL));
					if(kill_group_ret==-1){
						printf("This group does not exist\n");
					}
					else{
						//Find home ip and home port of a program insided group
						kill_program=find_ip_port(strtod(array_of_args[1],NULL));
						
						sock = socket(AF_INET, SOCK_STREAM, 0); 
						if (sock == -1) { 
							printf("TCP Socket creation failed\n"); 
							exit(1); 
						} 
						printf("kill program home ip=%s,port=%d\n",kill_program->home_ip,kill_program->home_port);
						//Update home
						kill_struct.type=htonl(HOME_KILL_MESSAGE);
						kill_struct.group_id=strtod(array_of_args[1],NULL);
						other_runtime.sin_family = AF_INET; 
						inet_aton(kill_program->home_ip,&other_runtime.sin_addr);
						other_runtime.sin_port = htons(kill_program->home_port); 
						if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
							printf("Connection with the other_runtime failed 1\n"); 
							exit(1); 
						} 
						send(sock,&kill_struct,sizeof(struct migrate_node),0);
						printf("send kill message to home\n");
						close(sock);
					}
				}
			}
			else if(strcmp(array_of_args[0],"migrate")==0){
				i=split_command(input,&array_of_args[0]);
				//Create migrate struct
				migrate_struct=(struct migrate_node *)malloc(sizeof(struct migrate_node));
				migrate_struct=search_and_copy(strtod(array_of_args[1],NULL),my_atoi(array_of_args[2]),array_of_args[3],my_atoi(array_of_args[4]),1,migrate_struct);
				//delete_program_after_migration(strtod(array_of_args[1],NULL),my_atoi(array_of_args[2]));
				
				sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				
				//UPDATE HOME
				strcpy(migrate_struct->new_migrate_ip,array_of_args[3]);
				migrate_struct->new_migrate_port=htonl(my_atoi(array_of_args[4]));
				migrate_struct->type=htonl(UPDATE_HOME);
				
				other_runtime.sin_family = AF_INET; 
				inet_aton(migrate_struct->home_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(ntohl(migrate_struct->home_port)); 
				if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
					printf("Connection with the other_runtime failed 1\n"); 
					exit(1); 
				} 
				printf("Send migrate struct to other runtime\n");
				send(sock,migrate_struct,sizeof(struct migrate_node),0);
				
				close(sock);
				sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				//MIGRATION
				other_runtime.sin_family = AF_INET; 
				inet_aton(array_of_args[3],&other_runtime.sin_addr);
				other_runtime.sin_port = htons(my_atoi(array_of_args[4])); 
				while(1){
					if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed \n"); 
					} 
					else{break;}
				}
				migrate_struct->type=htonl(MIGRATION);
				
				send(sock,migrate_struct,sizeof(struct migrate_node),0);
				close(sock);
				second_step_send=(struct second_step *)malloc(sizeof(struct second_step));
				sleep(2);
				//Send variables and messages
				second_step_send=search_and_copy_2nd_step(strtod(array_of_args[1],NULL),my_atoi(array_of_args[2]),array_of_args[3],my_atoi(array_of_args[4]),1,second_step_send);
				
				sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				//MIGRATION
				other_runtime.sin_family = AF_INET; 
				inet_aton(array_of_args[3],&other_runtime.sin_addr);
				other_runtime.sin_port = htons(my_atoi(array_of_args[4])); 
				while(1){
					if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed \n"); 
					} 
					else{break;}
				}
				//printf("Send second step message\n");
				send(sock,second_step_send,sizeof(struct second_step),0);
				
				close(sock);
				sleep(2);
				//Send variables and messages
				third_step_send=(struct third_step *)malloc(sizeof(struct third_step));
				third_step_send=search_and_copy_3nd_step(strtod(array_of_args[1],NULL),my_atoi(array_of_args[2]),array_of_args[3],my_atoi(array_of_args[4]),1,third_step_send);
				
				sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				//MIGRATION
				other_runtime.sin_family = AF_INET; 
				inet_aton(array_of_args[3],&other_runtime.sin_addr);
				other_runtime.sin_port = htons(my_atoi(array_of_args[4])); 
				while(1){
					if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed \n"); 
					} 
					else{break;}
				}
				//printf("Send third step message\n");
				send(sock,third_step_send,sizeof(struct second_step),0);
				delete_program_after_migration(strtod(array_of_args[1],NULL),my_atoi(array_of_args[2]));
				close(sock);
			
			}
			else if(strcmp(array_of_args[0],"shutdown")==0){
				
				//printf("Stutdown flag=1\n");
				shut_down_flag=1;
				sem_post(&balancing_mtx);
			}
		}
		sem_post(&str_tok_mtx);
	}
	return NULL;
}
//Receiver thread
void *my_receiver(){
	int sock,connfd,len,i,fd,bytes_returned,error,j,break_flag,sock_snd,program_id,migrate_port,load_balance_sock,kill_sock,position=0;
	double group_id;
	char array_of_variables[MAX_ROWS][MAX_SIZE],command[MAX_INPUT_SIZE];
	char migrate_ip[MAX_IP_SIZE];
	struct group_node *group=NULL,*current;
	struct program_node *program=NULL,*current_program;
	struct sockaddr_in my_net_info,other_runtime;
	struct migrate_node migrate_struct,*migrate_struct_balance,kill_struct;
	struct second_step second_step_send,*second_step_snd;
	struct third_step third_step_send,*third_step_snd;
	
	sock = socket(AF_INET, SOCK_STREAM, 0); 
	if (sock == -1) { 
		printf("my_receiver:TCP Socket creation failed\n"); 
		exit(1); 
	} 
	else{printf("my_receiver:TCP Socket successfully created\n"); }
	//Assign IP, PORT 
	my_net_info.sin_family = AF_INET;
	inet_aton(my_ip_address,&my_net_info.sin_addr);
	my_net_info.sin_port = htons(my_port); 
	if(bind(sock, (struct sockaddr*)&my_net_info, sizeof(my_net_info))<0){
		fprintf(stderr,"my_receiver:Binding TCP socket error.\n");
		exit(1);
	}
	if(listen(sock,5)<0){
		fprintf(stderr,"Listen() error.\n");
		exit(1);
	}
	while(1){
		connfd = accept(sock,(struct sockaddr*)&other_runtime,(socklen_t *)&len); 
		if (connfd < 0) { 
			printf("server acccept failed...\n"); 
			exit(0); 
		}
		recv(connfd,&migrate_struct,sizeof(struct migrate_node),0);
		if(load_balancing_flag==0){
			sem_wait(&str_tok_mtx);
		}
		if(ntohl(migrate_struct.type)==MIGRATION){
			printf("Got a migration message\n");
			set_array(array_of_variables);
			
			group=group_exists(migrate_struct.group_id);
			if(group==NULL){
				group=group_list_add(migrate_struct.group_id,0);
				if(my_port==ntohl(migrate_struct.home_port)){
					group->original=1;
				}
			}
			program=program_exists(migrate_struct.group_id,ntohl(migrate_struct.program_id));
			if(program!=NULL){
				group->program_list_head=delete_program(ntohl(migrate_struct.program_id),group->program_list_head);
				group->members--;
			}
			group->members++;
			program=program_list_add(group,ntohl(migrate_struct.program_id),group_id,migrate_struct.filename);
			program->home_port=ntohl(migrate_struct.home_port);
			strcpy(program->home_ip,migrate_struct.home_ip);
			program->program_id=ntohl(migrate_struct.program_id);
			program->migrate_bit=ntohl(migrate_struct.migrate_bit);
			program->group_id=migrate_struct.group_id;
			program->sleep_bit=ntohl(migrate_struct.sleep_bit);
			program->send_bit=ntohl(migrate_struct.send_bit);
			program->receive_bit=ntohl(migrate_struct.receive_bit);
			program->sleep_time=ntohl(migrate_struct.sleep_time);
			program->label_found=ntohl(migrate_struct.label_found);
			program->program_counter=ntohl(migrate_struct.program_counter);
			program->delete_bit=ntohl(migrate_struct.delete_bit);
			strcpy(program->filename,migrate_struct.filename);
			strcpy(program->command,migrate_struct.command);
			fd=open(migrate_struct.filename,O_CREAT|O_RDWR|O_EXCL,S_IRWXU);
			if (fd==-1){
				//File exists
			}
			
			else{
				//Copy program data
				i=0;
				while(1) {
					bytes_returned=write(fd,&(migrate_struct.mycode[i]),1);
					if(bytes_returned==0 || i==ntohl(migrate_struct.file_size)-1) {
						//Copy completed
						break;
					}
					i++;
				}
			}
			program->fd=fopen(migrate_struct.filename,"r+");
			while(1){
				bzero(command,MAX_INPUT_SIZE);
				fgets(command,MAX_INPUT_SIZE,program->fd);
				command[strlen(command)-1]='\0';
				
				if(strcmp(program->command,command)==0 && ftell(program->fd)==ntohl(migrate_struct.command_position)){
					fseek(program->fd,-strlen(command)-1,SEEK_CUR);
					break;
				}
			}
			connfd = accept(sock,(struct sockaddr*)&other_runtime,(socklen_t *)&len); 
			if (connfd < 0) { 
				printf("server acccept failed...\n"); 
				exit(0); 
			}
			recv(connfd,&second_step_send,sizeof(struct second_step),0);
			//printf("Receive second step message\n");
			position=0;
			int type,int_var;
			char name[20],str_var[20];
			//Copy variable list
			for(i=0;i<ntohl(second_step_send.num_of_variables);i++){
				memcpy(&type,&second_step_send.buffer[position],sizeof(int));
				position+=sizeof(int);
				
				memcpy(name,&second_step_send.buffer[position],20);
				position+=20;
				memcpy(&int_var,&second_step_send.buffer[position],sizeof(int));
				position+=sizeof(int);
				
				//printf("name=%s with value=%d\n",name,int_var);
				memcpy(str_var,&second_step_send.buffer[position],20);
				position+=20;
				
				if(type==INT_TYPE){
					program->variable_list_head=set_function(program->variable_list_head,"NULL",int_var,name,&error);
				}
				else{
					program->variable_list_head=set_function(program->variable_list_head,str_var,-1,name,&error);
				}
			}
			
			connfd = accept(sock,(struct sockaddr*)&other_runtime,(socklen_t *)&len); 
			if (connfd < 0) { 
				printf("server acccept failed...\n"); 
				exit(0); 
			}
			recv(connfd,&third_step_send,sizeof(struct third_step),0);
			//printf("Receive third step message\n");
			//Copy message list
			//printf("there are %d messages\n",ntohl(third_step_send.num_of_messages));
			for(i=0;i<ntohl(third_step_send.num_of_messages);i++){
				//printf("message list length is %d\n",ntohl(third_step_send.migrate_message_table[i].length));
				//printf("message is : ");
				for(j=0;j<ntohl(third_step_send.migrate_message_table[i].length);j++){
					//printf("message is %s ",third_step_send.migrate_message_table[i].array_of_variables[j]);
					strcpy(array_of_variables[j],third_step_send.migrate_message_table[i].array_of_variables[j]);
				}
				printf("\n");
				program->message_list_head=message_list_add_after_migration(program->message_list_head,ntohl(third_step_send.migrate_message_table[i].sender_id),ntohl(third_step_send.migrate_message_table[i].receiver_id),array_of_variables,ntohl(third_step_send.migrate_message_table[i].length));
				
			}
			
			printf("Migration completed\n");
			if(first_run==0){
				running_group=group;
				running_program=program;
				first_run=1;
				alarm(ALARM_TIME);
				sem_post(&mtx);
			}
		}
		else if(ntohl(migrate_struct.type)==UPDATE_HOME){
			printf("Got an update message\n");
			update_migrate_program(ntohl(migrate_struct.program_id),migrate_struct.group_id,ntohl(migrate_struct.new_migrate_port),migrate_struct.new_migrate_ip);
		}
		else if(ntohl(migrate_struct.type)==RET_MESSAGE){
			printf("Got a ret message\n");
			delete_program_after_ret(migrate_struct.group_id,ntohl(migrate_struct.program_id));
		}
		else if(ntohl(migrate_struct.type)==SEND_MESSAGE){
			printf("Got a send message\n");
			//Message from sender
			migrate_struct.sender_id=ntohl(migrate_struct.sender_id);
			migrate_struct.receiver_id=ntohl(migrate_struct.receiver_id);
			migrate_struct.length=ntohl(migrate_struct.length);
			//Check if receiver is in the same runtime
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(compare_float(current->group_id,migrate_struct.group_id)==1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->program_id==migrate_struct.receiver_id){
							if(current_program->migrate_bit==1){
								//Receiver is in different runtime,inform him
								sock_snd = socket(AF_INET, SOCK_STREAM, 0); 
								if (sock_snd == -1) { 
									printf("TCP Socket creation failed\n"); 
									exit(1); 
								}
								migrate_struct.type=htonl(SEND_MESSAGE);
								migrate_struct.sender_id=htonl(migrate_struct.sender_id);
								migrate_struct.receiver_id=htonl(migrate_struct.receiver_id);
								migrate_struct.length=htonl(migrate_struct.length);
								
								other_runtime.sin_family = AF_INET; 
								inet_aton(current_program->migrate_ip,&other_runtime.sin_addr);
								other_runtime.sin_port = htons(current_program->migrate_port); 
								if(connect(sock_snd,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
									printf("Connection with the other_runtime failed 1\n"); 
									exit(1); 
								} 
								send(sock_snd,&migrate_struct,sizeof(struct migrate_node),0);
								close(sock_snd);
								break_flag=1;
								break;
							}
							else{
								//Receiver is in the same runtime
								current_program->message_list_head=message_list_add(current_program->message_list_head,migrate_struct.sender_id,migrate_struct.receiver_id,migrate_struct.array_of_args,migrate_struct.length);
								if(current_program->receive_bit==1){
									//Receiver must execute the same command(RCV) again
									fseek(current_program->fd,-strlen(current_program->command)-1,SEEK_CUR);
									current_program->receive_bit=0;
								}
								break_flag=1;
								break;
								
							}
						}
					}
					if(break_flag==1){
						break_flag=0;
						break;
					}
				}
			}
		}
		else if(ntohl(migrate_struct.type)==SENDER_WAKE_UP){
			//Message from receiver
			migrate_struct.sender_id=ntohl(migrate_struct.sender_id);
			//Check if sender is in the same runtime
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(compare_float(current->group_id,migrate_struct.group_id)==1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->program_id==migrate_struct.sender_id){
							if(current_program->migrate_bit==1){
								//Sender is in different runtime,inform him
								sock_snd = socket(AF_INET, SOCK_STREAM, 0); 
								if (sock_snd == -1) { 
									printf("TCP Socket creation failed\n"); 
									exit(1); 
								} 
								migrate_struct.type=htonl(SENDER_WAKE_UP);
								migrate_struct.sender_id=htonl(migrate_struct.sender_id);
								
								other_runtime.sin_family = AF_INET; 
								inet_aton(current_program->migrate_ip,&other_runtime.sin_addr);
								other_runtime.sin_port = htons(current_program->migrate_port); 
								if(connect(sock_snd,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
									printf("Connection with the other_runtime failed 1\n"); 
									exit(1); 
								} 
								send(sock_snd,&migrate_struct,sizeof(struct migrate_node),0);
								close(sock_snd);
								break_flag=1;
								break;
							}
							else{
								//Sender is in the same runtime
								current_program->send_bit=0;
								break_flag=1;
								break;
							}
						}
					}
					if(break_flag==1){
						break_flag=0;
						break;
					}
				}
			}
		}
		else if(ntohl(migrate_struct.type)==LOAD_BALANCE_MIGRATION){
			printf("Received a migration message.I must migrate %d programms to ip %s and port %d\n",ntohl(migrate_struct.length),migrate_struct.home_ip,ntohl(migrate_struct.home_port));
			bzero(migrate_ip,20);
			migrate_port=ntohl(migrate_struct.home_port);
			strcpy(migrate_ip,migrate_struct.home_ip);
			for(i=0;i<ntohl(migrate_struct.length);i++){
				find_a_process_to_migrate(&group_id,&program_id);
				printf("migrate program %d from group %lf\n",program_id,group_id);
				migrate_struct_balance=(struct migrate_node *)malloc(sizeof(struct migrate_node));
				migrate_struct_balance=search_and_copy(group_id,program_id,migrate_ip,migrate_port,1,migrate_struct_balance);
				load_balance_sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (load_balance_sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				
				//UPDATE HOME
				strcpy(migrate_struct_balance->new_migrate_ip,migrate_ip);
				migrate_struct_balance->new_migrate_port=htonl(migrate_port);
				migrate_struct_balance->type=htonl(UPDATE_HOME);
				
				other_runtime.sin_family = AF_INET; 
				inet_aton(migrate_struct_balance->home_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(ntohl(migrate_struct_balance->home_port)); 
				if(connect(load_balance_sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
					printf("Connection with the other_runtime failed 1\n"); 
					exit(1); 
				} 
				send(load_balance_sock,migrate_struct_balance,sizeof(struct migrate_node),0);
				
				close(load_balance_sock);
				load_balance_sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (load_balance_sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				//MIGRATION
				other_runtime.sin_family = AF_INET; 
				inet_aton(migrate_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(migrate_port); 
				while(1){
					if(connect(load_balance_sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed \n"); 
					} 
					else{break;}
				}
				migrate_struct_balance->type=htonl(MIGRATION);
				
				send(load_balance_sock,migrate_struct_balance,sizeof(struct migrate_node),0);
				close(load_balance_sock);
				sleep(2);
				load_balance_sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (load_balance_sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				//MIGRATION
				other_runtime.sin_family = AF_INET; 
				inet_aton(migrate_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(migrate_port); 
				while(1){
					if(connect(load_balance_sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed \n"); 
					} 
					else{break;}
				}
				//printf("Sending 2nd step message\n");
				second_step_snd=(struct second_step *)malloc(sizeof(struct second_step));
				second_step_snd=search_and_copy_2nd_step(group_id,program_id,migrate_ip,migrate_port,1,second_step_snd);
				send(load_balance_sock,second_step_snd,sizeof(struct second_step),0);
				close(load_balance_sock);
				
				
				load_balance_sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (load_balance_sock == -1) { 
					printf("TCP Socket creation failed\n"); 
					exit(1); 
				} 
				//MIGRATION
				other_runtime.sin_family = AF_INET; 
				inet_aton(migrate_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(migrate_port); 
				while(1){
					if(connect(load_balance_sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
						printf("Connection with the other_runtime failed \n"); 
					} 
					else{break;}
				}
				//printf("Sending 3d step message\n");
				third_step_snd=(struct third_step *)malloc(sizeof(struct third_step));
				third_step_snd=search_and_copy_3nd_step(group_id,program_id,migrate_ip,migrate_port,1,third_step_snd);
				send(load_balance_sock,third_step_snd,sizeof(struct third_step),0);
				close(load_balance_sock);
				
				delete_program_after_migration(group_id,program_id);
			}
			i=1;
			send(connfd,&i,sizeof(int),0);
		}
		else if(ntohl(migrate_struct.type)==HOME_KILL_MESSAGE){
			
			printf("Received a home kill message\n");
			//If there are migrated programms,inform their runtime with a KILL_MESSAGE
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(compare_float(current->group_id,migrate_struct.group_id)==1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->migrate_bit==1){
							kill_sock = socket(AF_INET, SOCK_STREAM, 0); 
							if (kill_sock == -1) { 
								printf("TCP Socket creation failed\n"); 
								exit(1); 
							} 
							other_runtime.sin_family = AF_INET; 
							inet_aton(current_program->migrate_ip,&other_runtime.sin_addr);
							other_runtime.sin_port = htons(current_program->migrate_port); 
							while(1){
								if(connect(kill_sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
									printf("Connection with the other_runtime failed \n"); 
								} 
								else{break;}
							}
							kill_struct.type=htonl(KILL_MESSAGE);
							kill_struct.group_id=current->group_id;
							kill_struct.program_id=htonl(current_program->program_id);
							send(kill_sock,&kill_struct,sizeof(struct migrate_node),0);
							close(kill_sock);
						}
					}
				}
			}
			//After informing all migrated programs,delete group
			kill_group(migrate_struct.group_id);
		}
		else if(ntohl(migrate_struct.type)==KILL_MESSAGE){
			
			printf("Received a kill message from home.Kill program %d\n",ntohl(migrate_struct.program_id));
			//Received a KILL_MESSAGE from home runtime.Mark delete_bit to 1,for the delete program
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(compare_float(current->group_id,migrate_struct.group_id)==1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->program_id==ntohl(migrate_struct.program_id)){
							kill_program(ntohl(migrate_struct.program_id),current->program_list_head);
							break;
						}
					}
				}
			}
		}
		else if(ntohl(migrate_struct.type)==CHANGE_HOME_IP){
			printf("Got a shutdown message.Program %d has new home ip %s and new home port %d\n",ntohl(migrate_struct.program_id),migrate_struct.home_ip,ntohl(migrate_struct.home_port));
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(compare_float(current->group_id,migrate_struct.group_id)==1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->program_id==ntohl(migrate_struct.program_id)){
							strcpy(current_program->home_ip,migrate_struct.home_ip);
							if(strcmp(my_ip_address,current_program->home_ip)==0){
								current_program->migrate_bit=0;
							}
							current_program->home_port=ntohl(migrate_struct.home_port);
							break;
						}
					}
				}
			}
		}
		close(connfd);
		if(load_balancing_flag==0){
			sem_post(&str_tok_mtx);
		}
	}
	return NULL;
}
void *balancing_sender(){
	struct sockaddr_in group_sock,receive_info,other_runtime;
	struct multicast_message multicast_msg;
	struct timeval timeout;
	struct balance_node *min,*max;
	struct migrate_node migrate_struct,shut_down_struct,*migrate_shut_down_struct;
	int sock,receive_sock,connfd,len,avg_value,ack,shut_down_sock;
	struct group_node *current;
	struct program_node *current_program;
	struct second_step *second_step_snd;
	struct third_step *third_step_snd;
	
	receive_sock = socket(AF_INET, SOCK_STREAM, 0); 
	if (receive_sock == -1) { 
		printf("balancing_sender:TCP Socket creation failed\n"); 
		exit(1); 
	} 
	else{printf("balancing_sender:TCP Socket successfully created\n"); }
	//Assign IP, PORT 
	receive_info.sin_family = AF_INET;
	inet_aton(my_ip_address,&receive_info.sin_addr);
	receive_info.sin_port = htons(my_port+1); 
	timeout.tv_sec = 4;
	timeout.tv_usec = 0;
	setsockopt(receive_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	
	if(bind(receive_sock, (struct sockaddr*)&receive_info, sizeof(receive_info))<0){
		fprintf(stderr,"balancing_sender:Binding TCP socket error.\n");
		exit(1);
	}
	if(listen(receive_sock,5)<0){
		fprintf(stderr,"Listen() error.\n");
		exit(1);
	}
	
	while(1){
		sem_wait(&balancing_mtx);
		memset(&other_runtime,0,sizeof(struct sockaddr_in));
		//Send load balancing message to multicast address and multicast port
		sock = socket(AF_INET, SOCK_DGRAM, 0); 
		if (sock == -1) { 
			printf("balancing_sender:TCP Socket creation failed\n"); 
			exit(1); 
		} 
		
		group_sock.sin_family = AF_INET; 
		inet_aton("226.1.1.1",&group_sock.sin_addr);
		group_sock.sin_port = htons(10000); 
		multicast_msg.type=htonl(MULTICAST_TYPE);
		multicast_msg.port=htonl(my_port+1);
		strcpy(multicast_msg.ip,my_ip_address);
		
		if(sendto(sock,&multicast_msg,sizeof(struct multicast_message),0,(struct sockaddr*)&group_sock,sizeof(group_sock)) < 0){fprintf(stderr,"Sending balancing message error");}
		else{printf("Sending balancing message...OK\n");}
		close(sock);
		
		while(1){
			//Get total processes number from all other runtimes
			connfd = accept(receive_sock,(struct sockaddr*)&other_runtime,(socklen_t *)&len); 
			if (connfd < 0) { 
				break;
			}
			if(recv(connfd,&migrate_struct,sizeof(struct migrate_node),0)<0){
				break;
			}
			printf("total_processes=%d from ip %s and %d port\n",ntohl(migrate_struct.length),migrate_struct.home_ip,ntohl(migrate_struct.home_port));
			balance_list_add(migrate_struct.home_ip,ntohl(migrate_struct.home_port),ntohl(migrate_struct.length));
			memset(&migrate_struct,0,sizeof(struct migrate_node));
		}
		//sem_wait(&str_tok_mtx);
		close(connfd);
		if(shut_down_flag==1){
			alarm(1000);
			printf("Begin shutdown\n");
			min=find_min_for_shutdown(my_port,my_ip_address);
			//Inform all migrated processes about theri new home(min)
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(current->group_id!=1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->migrate_bit==1){
							printf("Update %lf.%d program.New home ip is %s and new home port is %d\n",current_program->group_id,current_program->program_id,min->home_ip,min->home_port);
							shut_down_sock = socket(AF_INET, SOCK_STREAM, 0); 
							if (shut_down_sock == -1) { 
								printf("TCP Socket creation failed\n"); 
								exit(1); 
							} 
							other_runtime.sin_family = AF_INET; 
							inet_aton(current_program->migrate_ip,&other_runtime.sin_addr);
							other_runtime.sin_port = htons(current_program->migrate_port); 
							while(1){
								if(connect(shut_down_sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
									printf("Connection with the other_runtime failed \n"); 
								} 
								else{break;}
							}
							shut_down_struct.type=htonl(CHANGE_HOME_IP);
							shut_down_struct.group_id=current->group_id;
							strcpy(shut_down_struct.home_ip,min->home_ip);
							shut_down_struct.home_port=htonl(min->home_port);
							shut_down_struct.program_id=htonl(current_program->program_id);
							send(shut_down_sock,&shut_down_struct,sizeof(struct migrate_node),0);
							close(shut_down_sock);
						}
					}
				}
			}
			//Migrate all programms to min runtime
			for(current=group_list_head->next;;current=current->next){
				if(current==group_list_head){break;}
				if(current->group_id!=1){
					for(current_program=current->program_list_head->next;;current_program=current_program->next){
						if(current_program==current->program_list_head){break;}
						if(current_program->migrate_port!=min->home_port){
							migrate_shut_down_struct=(struct migrate_node *)malloc(sizeof(struct migrate_node));
							migrate_shut_down_struct=search_and_copy(current->group_id,current_program->program_id,current_program->migrate_ip,current_program->migrate_port,current_program->migrate_bit,migrate_shut_down_struct);
							migrate_shut_down_struct->home_port=htonl(min->home_port);
							strcpy(migrate_shut_down_struct->home_ip,min->home_ip);
							
							sock = socket(AF_INET, SOCK_STREAM, 0); 
							if (sock == -1) { 
								printf("TCP Socket creation failed\n"); 
								exit(1); 
							} 
							//MIGRATION
							other_runtime.sin_family = AF_INET; 
							inet_aton(min->home_ip,&other_runtime.sin_addr);
							other_runtime.sin_port = htons(min->home_port); 
							if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
								printf("Connection with the other_runtime failed \n"); 
							}
							migrate_shut_down_struct->type=htonl(MIGRATION);
							
							send(sock,migrate_shut_down_struct,sizeof(struct migrate_node),0);
							close(sock);
							sleep(2);
							
							sock = socket(AF_INET, SOCK_STREAM, 0); 
							if (sock == -1) { 
								printf("TCP Socket creation failed\n"); 
								exit(1); 
							} 
							//MIGRATION
							other_runtime.sin_family = AF_INET; 
							inet_aton(min->home_ip,&other_runtime.sin_addr);
							other_runtime.sin_port = htons(min->home_port); 
							if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
								printf("Connection with the other_runtime failed \n"); 
							}
							
							second_step_snd=(struct second_step *)malloc(sizeof(struct second_step));
							second_step_snd=search_and_copy_2nd_step(current->group_id,current_program->program_id,current_program->migrate_ip,current_program->migrate_port,current_program->migrate_bit,second_step_snd);
							send(sock,second_step_snd,sizeof(struct second_step),0);
							close(sock);
							//printf("Sending 2nd step message\n");
							
							sleep(2);
							
							sock = socket(AF_INET, SOCK_STREAM, 0); 
							if (sock == -1) { 
								printf("TCP Socket creation failed\n"); 
								exit(1); 
							} 
							//MIGRATION
							other_runtime.sin_family = AF_INET; 
							inet_aton(min->home_ip,&other_runtime.sin_addr);
							other_runtime.sin_port = htons(min->home_port); 
							if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
								printf("Connection with the other_runtime failed \n"); 
							}
							
							third_step_snd=(struct third_step *)malloc(sizeof(struct third_step));
							third_step_snd=search_and_copy_3nd_step(current->group_id,current_program->program_id,current_program->migrate_ip,current_program->migrate_port,current_program->migrate_bit,third_step_snd);
							send(sock,third_step_snd,sizeof(struct third_step),0);
							close(sock);
							//printf("Sending 3d step message\n");
							
							sleep(2);
						}
					}
				}
			}
			
			//Inform all runtimes that shutdown procedure has over
			sock = socket(AF_INET, SOCK_DGRAM, 0); 
			if (sock == -1) { 
				printf("balancing_sender:UDP multicast Socket creation failed\n"); 
				exit(1); 
			}
			
			group_sock.sin_family = AF_INET; 
			inet_aton("226.1.1.1",&group_sock.sin_addr);
			group_sock.sin_port = htons(10000); 
			multicast_msg.type=htonl(LOAD_BALANCE_END);
			
			if(sendto(sock,&multicast_msg,sizeof(struct multicast_message),0,(struct sockaddr*)&group_sock,sizeof(group_sock)) < 0){fprintf(stderr,"Sending balancing message error");}
			else{printf("Sending shutdown ending message...OK\n");}
			close(sock);
			balance_list_delete();
			
			printf("End shutdown\n");
			kill(getpid(),SIGINT);
		}
		else{
			//Compute average value
			avg_value=find_avg_from_balance_list();
			
			while(1){
				//If there is a gap,restart load balance
				if(find_gap(avg_value,GAP)==1){break;}
				min=find_min();
				max=find_max();
				
				migrate_struct.type=ntohl(LOAD_BALANCE_MIGRATION);
				migrate_struct.length=ntohl(avg_value-min->total_processes);
				migrate_struct.home_port=ntohl(min->home_port);
				strcpy(migrate_struct.home_ip,min->home_ip);
				if(avg_value-min->total_processes==0){break;}
				
				max->total_processes-=avg_value-min->total_processes;
				min->total_processes=avg_value;
				
				//Send the migrate_struct to max.Migrate struct contains min's ip,port and migrate number
				sock = socket(AF_INET, SOCK_STREAM, 0); 
				if (sock == -1) { 
					printf("balancing_sender :TCP Socket creation failed\n"); 
					exit(1); 
				}
				
				other_runtime.sin_family = AF_INET; 
				inet_aton(max->home_ip,&other_runtime.sin_addr);
				other_runtime.sin_port = htons(max->home_port); 
				if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
					printf("balancing_sender : Connection with the other_runtime failed \n"); 
					exit(1); 
				} 
				send(sock,&migrate_struct,sizeof(struct migrate_node),0);
				//Migration step is over
				recv(sock,&ack,sizeof(int),0);
				printf("Received ack from max,continue to migration next step\n");
				close(sock);
			}
			//Inform all runtimes that load balancing procedure has over
			sock = socket(AF_INET, SOCK_DGRAM, 0); 
			if (sock == -1) { 
				printf("balancing_sender:UDP multicast Socket creation failed\n"); 
				exit(1); 
			}
			
			group_sock.sin_family = AF_INET; 
			inet_aton("226.1.1.1",&group_sock.sin_addr);
			group_sock.sin_port = htons(10000); 
			multicast_msg.type=htonl(LOAD_BALANCE_END);
			
			if(sendto(sock,&multicast_msg,sizeof(struct multicast_message),0,(struct sockaddr*)&group_sock,sizeof(group_sock)) < 0){fprintf(stderr,"Sending balancing message error");}
			else{printf("Sending load balancing ending message...OK\n");}
			close(sock);
            sleep(10);
			balance_list_delete();
		}
	}
}
void *multicast_receiver(){
	int multicast_sock,fromlen,total_processes,sock;
	struct sockaddr_in multicast_info,from,other_runtime;
	struct ip_mreq group;
	struct multicast_message multicast_msg;
	struct migrate_node migrate_struct; 
	
	multicast_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(multicast_sock < 0){
		fprintf(stderr,"multicast_receiver:Opening datagram socket error.\n");
		exit(1);
	}
	fromlen=sizeof(struct sockaddr_in);
	
	u_int yes = 1;
	/*Multiple servers can receive requests to the same port*/
	if(setsockopt(multicast_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){
		perror("multicast_receiver:Reusing ADDR failed");
		return NULL;
	}
	
	multicast_info.sin_family = AF_INET;
	multicast_info.sin_port = htons(10000);
	multicast_info.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(multicast_sock, (struct sockaddr*)&multicast_info, sizeof(multicast_info))){
		fprintf(stderr,"multicast_receiver:Binding datagram socket error.\n");
		exit(1);
	}
	
	fromlen=sizeof(struct sockaddr_in);
	group.imr_multiaddr.s_addr=inet_addr("226.1.1.1");
	group.imr_interface.s_addr = htonl(INADDR_ANY);  
	/*Adding membership from multicast group*/
	if(setsockopt(multicast_sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&group, sizeof(group)) < 0){
		perror("multicast_receiver:Adding multicast group error");
		exit(1);
	}
	
	while(1){
		//Receive a load balance message from multicast group
		if(recvfrom(multicast_sock,&multicast_msg,sizeof(struct multicast_message),0,(struct sockaddr *)&from,(socklen_t *)&fromlen)<=0){printf("An error has occured in recvfrom\n");}
		else{printf("Got a multicast message\n");}
		if(ntohl(multicast_msg.type)==LOAD_BALANCE_END){
			load_balancing_counter=0;
			load_balancing_flag=0;
			sem_post(&str_tok_mtx);
		}
		else{
			//Inform load balancer about the total processes value of this runtime
			load_balancing_flag=1;
			load_balancing_counter=0;
			sem_wait(&str_tok_mtx);
			total_processes=find_total_processes();
			total_processes--;
			
			sock = socket(AF_INET, SOCK_STREAM, 0); 
			if (sock == -1) { 
				printf("multicast_receiver :TCP Socket creation failed\n"); 
				exit(1); 
			}
			migrate_struct.type=htonl(MULTICAST_ANSWER);
			migrate_struct.home_port=htonl(my_port);
			strcpy(migrate_struct.home_ip,my_ip_address);
			migrate_struct.length=htonl(total_processes);
			
			other_runtime.sin_family = AF_INET; 
			inet_aton(multicast_msg.ip,&other_runtime.sin_addr);
			other_runtime.sin_port = htons(ntohl(multicast_msg.port)); 
			if(connect(sock,(struct sockaddr *)&other_runtime, sizeof(other_runtime)) != 0){ 
				printf("multicast_receiver : Connection with the other_runtime failed \n"); 
				exit(1); 
			} 
			send(sock,&migrate_struct,sizeof(struct migrate_node),0);
		}
	}
}
int split_command(char *input,char array_of_args[MAX_ROWS][MAX_SIZE]){
	
	int i,j=0,size_of_array_of_token=0;
	for(i=0;i<=strlen(input);i++){
		if((input[i]==' ')|| i==strlen(input)){ // if you find letters , move on
			if(input[i-1]!=' '){ // if you find a letter , check if the previous symbol is a letter or a space
				// if it is a letter copy the (i-j) characters ,starting from &input[j],to array_of_args[array_of_token]
				strncpy(array_of_args[size_of_array_of_token],&input[j],i-j);
				array_of_args[size_of_array_of_token][i-j+1]='\0'; // put \0 in the i-j+1 so it can be a string.
				size_of_array_of_token++;
				j=i+1; // j goes along with i becaues we want to iterate this copy of characters
			}
			else{j++;} // if you find space and the previous is space , move j so it can be alongside with i
		}
	}
	return(size_of_array_of_token);
}
int find_members(char array_of_args[MAX_ROWS][MAX_SIZE],int length){
	
	int i=0,members=0;
	for(i=0;i<length;i++){
		if(strcmp(array_of_args[i],"||")==0){
			members++;
		}
	}
	return(members+1);
	
}
