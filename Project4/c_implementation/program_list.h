#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define UPDATE_HOME -14
#define MIGRATION -15
#define RET_MESSAGE -16
#define SEND_MESSAGE -17
#define SENDER_WAKE_UP -18
#define MULTICAST_TYPE -19
#define MULTICAST_ANSWER -20
#define LOAD_BALANCE_MIGRATION -21
#define LOAD_BALANCE_END -22
#define KILL_MESSAGE -23
#define HOME_KILL_MESSAGE -24
#define CHANGE_HOME_IP -25
#define MAX_ROWS 40
#define MAX_SIZE 20
#define MAX_INPUT_SIZE 200
#define MAX_IP_SIZE 20
#include "message_list.h"
struct program_node{
	int program_id;
	int home_port;
	char home_ip[MAX_IP_SIZE];
	int migrate_port;
	char migrate_ip[MAX_IP_SIZE];
	double group_id;
	int migrate_bit;
	int sleep_bit;
	int send_bit;
	int receive_bit;
	double sleep_time;
	int label_found;
	int program_counter;
	int delete_bit;
	char label[MAX_IP_SIZE];
	char filename[MAX_IP_SIZE];
	char * token;
	char command[MAX_INPUT_SIZE];
	FILE * fd;
	struct program_node *next;
	struct program_node *prev;
	struct var_node *variable_list_head;
	struct message_node *message_list_head;
};
//Migrate basic state bytes
struct migrate_node{
	int type;
	//*****update message*****//
	int new_migrate_port;
	char new_migrate_ip[MAX_IP_SIZE];
	//*****update message*****//
	int program_id;
	int file_size;
	int home_port;
	char home_ip[MAX_IP_SIZE];
	double group_id;
	int sleep_bit;
	long int command_position;
	int send_bit;
	int receive_bit;
	double sleep_time;
	int label_found;
    int migrate_bit;
	int program_counter;
	int delete_bit;
	char label[MAX_IP_SIZE];
	char filename[MAX_IP_SIZE];
	char token[MAX_IP_SIZE];
	char command[MAX_INPUT_SIZE];
	char mycode[500];
	int sender_id;
	int receiver_id;
	char array_of_args[10][MAX_SIZE];
	int length;
};
//Migrate variable list
struct second_step{
	int num_of_variables;
	char buffer[(2*sizeof(int)+20+20)*20];
};
//Migrate message list
struct third_step{
	
	int num_of_messages;
	struct message_node migrate_message_table[1];
};
struct multicast_message{
	int type;
	int port;
	char ip[MAX_IP_SIZE];
};
struct group_node{
	double group_id;
	int original;
	int members;
	int delete_bit;
	struct group_node *next;
	struct group_node *prev;
	struct program_node *program_list_head;
};
//Initialize the program list
struct program_node * program_list_init(struct program_node * head){
	head=(struct program_node *)malloc(sizeof(struct program_node));
	head->next=head;
	head->prev=head;
	return(head);
}
//Add a program node.Create a variable list inside this node
struct program_node * program_list_add(struct group_node *group,int program_id,double group_id,char *program_name){
	struct program_node *new_node=(struct program_node *)malloc(sizeof(struct program_node));
	new_node->program_id=program_id;
	new_node->group_id=group_id;
	new_node->program_counter=0;
	new_node->sleep_bit=0;
	new_node->migrate_bit=0;
	new_node->send_bit=0;
	new_node->receive_bit=0;
	new_node->label_found=0;
	new_node->sleep_time=0;
	new_node->delete_bit=0;
	new_node->token=(char *)malloc(MAX_SIZE);
	strcpy(new_node->filename,program_name);
	new_node->variable_list_head=variable_list_init(new_node->variable_list_head);
	new_node->message_list_head=message_list_init(new_node->message_list_head);
	
	new_node->next=group->program_list_head->next;
	group->program_list_head->next->prev=new_node;
	group->program_list_head->next=new_node;
	new_node->prev=group->program_list_head;
	return new_node;
}
//Delete a program node
void delete_program_list(struct program_node *program_list_head){
	//Delete the variable list
	struct program_node *current,*temp;
	
	for(current=program_list_head->next;;){
		if(current==program_list_head){break;}
		temp=current->next;
		delete_variable_list(current->variable_list_head);
		current->prev->next=current->next;
		current->next->prev=current->prev;
		free(current);
		current=temp;
	}
	free(program_list_head);
	
}
void kill_program(int id,struct program_node *program_list_head){
	struct program_node *current;
	
	for(current=program_list_head->next;;current=current->next){
		if(current==program_list_head){break;}
		if(current->program_id==id){
			current->delete_bit=1;
			break;
		}
	}
}
struct program_node * delete_program(int id,struct program_node *program_list_head){
	struct program_node *current;
	
	for(current=program_list_head->next;;current=current->next){
		if(current==program_list_head){break;}
		if(current->program_id==id){
			current->prev->next=current->next;
			current->next->prev=current->prev;
			free(current);
			break;
			
		}
	}
	return(program_list_head);
}

void print_list(struct program_node *program_list_head,double groupid){
	struct program_node *current;
	
	for(current=program_list_head->next;;current=current->next){
		if(current==program_list_head){break;}
		if(current->migrate_bit==0){
			printf(ANSI_COLOR_RED"ID[%lf.%d] executes %s\n"ANSI_COLOR_RESET,groupid,current->program_id,current->filename);
		}
		else{
			printf(ANSI_COLOR_RED"ID[%lf.%d] executes %s [migrated to ip:%s and port:%d]\n"ANSI_COLOR_RESET,groupid,current->program_id,current->filename,current->migrate_ip,current->migrate_port);
		}
	}
}
void fix_message_arguments_for_migration_send(struct program_node * running_program,char array_of_token[MAX_ROWS][MAX_SIZE],int length){
	
	struct var_node* current;
	char str[MAX_SIZE];
	int i=0;
	
	while(i<length){
		if(array_of_token[i][0]=='$'){
			for(current=running_program->variable_list_head->next;;current=current->next){
				if(current==running_program->variable_list_head){break;}
				if(strcmp(array_of_token[i],current->variable_name)==0){
					if(current->type==INT_TYPE){
						sprintf(str, "%d",current->int_variable);
						strcpy(array_of_token[i],str);
					}
					else{
						strcpy(array_of_token[i],current->string_variable);
					}
					break;
				}
			}
		}
		i++;
	}
}
struct program_node *snd_function(struct group_node *running_group,struct program_node * running_program,char array_of_token[MAX_ROWS][MAX_SIZE],int *error,int length,int *blocking){
		
	struct program_node *current_program;
	struct var_node* current;
	char str[MAX_SIZE];
	int i=0;
	
	//If there is a defined variable inside the command,replace the variable with his value/string 
	while(i<length){
		if(array_of_token[i][0]=='$'){
			for(current=running_program->variable_list_head->next;;current=current->next){
				if(current==running_program->variable_list_head){break;}
				if(strcmp(array_of_token[i],current->variable_name)==0){
					if(current->type==INT_TYPE){
						sprintf(str, "%d",current->int_variable);
						strcpy(array_of_token[i],str);
					}
					else{
						strcpy(array_of_token[i],current->string_variable);
					}
					break;
				}
			}
		}
		i++;
	}
	
	//Add the message to receiver program list.If the receiver is blocked inside receive function,wake up the receiver
	for(current_program=running_group->program_list_head->next;;current_program=current_program->next){
		if(current_program==running_group->program_list_head){
			running_program->send_bit=1;
			*blocking=1;
			break;
		}
		if(current_program->program_id==my_atoi(array_of_token[0])){
			current_program->message_list_head=message_list_add(current_program->message_list_head,running_program->program_id,current_program->program_id,array_of_token,i);
			if(current_program->receive_bit==1){
				//Receiver must execute the same command(RCV) again
				fseek(current_program->fd,-strlen(current_program->command)-1,SEEK_CUR);
				current_program->receive_bit=0;
			}
			running_program->send_bit=1;
			*blocking=1;
			break;
		}
	}
	return(running_program);
}
struct program_node * rcv_function(struct group_node *running_group,struct program_node *running_program,char array_of_token[MAX_ROWS][MAX_SIZE],int *error,int length,int *blocking){
	
	int i=0,exists_ret=0,integer_value=0,set_error,match=0;
	char string_value[MAX_SIZE];
	struct program_node *sender;
	struct message_node *current;
	struct var_node* current_variable;
	//Search in the message list for messages.Sender must be my_atoi(array_of_token[1])
	for(current=running_program->message_list_head->next;;current=current->next){
		if(current==running_program->message_list_head){
			printf(ANSI_COLOR_GREEN"ID->[%lf ---> %d]Not found message,rcv is blocking\n"ANSI_COLOR_RESET,running_group->group_id,running_program->program_id);
			*blocking=1;
			running_program->receive_bit=1;
			return(running_program);
		}
		if(running_program->program_id==current->receiver_id && my_atoi(array_of_token[0])==current->sender_id){
			//Found a message in the list
			for(i=1;i<length;i++){
				//All arguments of SND and RCV functions should match
				if(array_of_token[i][0]!='$' && array_of_token[i][0]!='"' && my_atoi(array_of_token[i])==my_atoi(current->array_of_variables[i-1])){}
				else if(array_of_token[i][0]=='$' &&((my_atoi(current->array_of_variables[i-1])!=0) ||  strcmp(current->array_of_variables[i-1],"0")==0)){
					//RCV->has a variable(dont know the type yet),SND->integer
					exists_ret=check_var_exists(running_program->variable_list_head,array_of_token[i],string_value,&integer_value);
					if(exists_ret==-1){
						running_program->variable_list_head=set_function(running_program->variable_list_head,"NULL",my_atoi(current->array_of_variables[i-1]),array_of_token[i],&set_error);
					}
					else if(exists_ret==0){
						printf("SND->integer and RCV variable has string type,rcv program is blocking\n");
						*blocking=1;
						running_program->receive_bit=1;
						return(running_program);
					}
					else{
						for(current_variable=running_program->variable_list_head->next;;current_variable=current_variable->next){
							if(current_variable==running_program->variable_list_head){break;}
							if(strcmp(current_variable->variable_name,array_of_token[i])==0){
								current_variable->int_variable=my_atoi(current->array_of_variables[i-1]);
								break;
							}
						}
					}
				}
				else if(array_of_token[i][0]=='$' && my_atoi(current->array_of_variables[i-1])==0 ){
					//RCV->has a variable(dont know the type yet),SND->string
					exists_ret=check_var_exists(running_program->variable_list_head,array_of_token[i],string_value,&integer_value);
					if(exists_ret==-1){
						running_program->variable_list_head=set_function(running_program->variable_list_head,current->array_of_variables[i-1],-1,array_of_token[i],&set_error);
					}
					else if(exists_ret==0){
						for(current_variable=running_program->variable_list_head->next;;current_variable=current_variable->next){
							if(current_variable==running_program->variable_list_head){break;}
							if(strcmp(current_variable->variable_name,array_of_token[i])==0){
								strcpy(current_variable->string_variable,current->array_of_variables[i-1]);
								break;
							}
						}
					}
					else{
						printf("SND->string and RCV variable has integer type,rcv program is blocking\n");
						running_program->receive_bit=1;
						*blocking=1;
						return(running_program);
					}
				}
				else if(array_of_token[i][0]=='"' && current->array_of_variables[i-1][0]=='"' && strcmp(array_of_token[i],current->array_of_variables[i-1])==0){}
				else{
					printf("Receive and snd do not match,rcv program is blocking\n");
					running_program->receive_bit=1;
					*blocking=1;
					return(running_program);
				}
			}
			running_program->receive_bit=0;
			//Search in the program list for the sender,update sender_bit to 0
			for(sender=running_group->program_list_head->next;;sender=sender->next){
				if(sender==running_group->program_list_head){
					printf("Sender has migrated to other runtime\n");
					*blocking=2;
					break;
				}
				if(sender->program_id==my_atoi(array_of_token[0]) && sender->migrate_bit!=1){
					sender->send_bit=0;
					break;
				}
			}
			message_list_delete(running_program->message_list_head,my_atoi(array_of_token[0]));
			match=1;
			//printf("Matched messages\n");
		}
		if(match==1){break;}
	}
	return(running_program);
}
