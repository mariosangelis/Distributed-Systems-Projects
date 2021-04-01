struct group_node *group_list_head;
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#include "program_list.h"
#include "my_semaphores.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
int lock;
int compare_float(float f1, float f2);
//Initialize the program list
void group_list_init(){
	lock=mysem_create(lock,1);
	group_list_head=(struct group_node *)malloc(sizeof(struct group_node));
	group_list_head->next=group_list_head;
	group_list_head->prev=group_list_head;
}
//Add a program node.Create a variable list inside this node
struct group_node * group_list_add(double id,int members){
	mysem_down(lock);
	struct group_node *new_node=(struct group_node *)malloc(sizeof(struct group_node));
	new_node->group_id=id;
	new_node->delete_bit=0;
	new_node->members=members;
	new_node->program_list_head=program_list_init(new_node->program_list_head);
	
	new_node->next=group_list_head->next;
	group_list_head->next->prev=new_node;
	group_list_head->next=new_node;
	new_node->prev=group_list_head;
	mysem_up(lock);
	return new_node;
}
//Set the delete_bit of the group with group_id=id to 1
int kill_group(double id){
	mysem_down(lock);
	struct group_node *current;
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){
			mysem_up(lock);
			return(-1);
		}
		if(compare_float(current->group_id,id)==1){
			current->delete_bit=1;
			mysem_up(lock);
			return 0;
		}
	}
}
//Print the group list
void print_group_list(){
	mysem_down(lock);
	struct group_node *current;
	printf("========== LIST OF GROUPS ==========\n");
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		printf(ANSI_COLOR_BLUE"-----GROUP[%lf] : MEMBERS : %d-----\n"ANSI_COLOR_RESET,current->group_id,current->members);
		print_list(current->program_list_head,current->group_id);
	}
	printf("=====================================\n");
	mysem_up(lock);
}
void update_sleep_time(long int prev_time,long int current_time){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		for(current_program=current->program_list_head->next;;current_program=current_program->next){
			if(current_program==current->program_list_head){break;}
			if(current_program->sleep_bit==1){
				current_program->sleep_time-=(current_time-prev_time);
				if(current_program->sleep_time<=0){
					current_program->sleep_time=0;
					current_program->sleep_bit=0;
				}
			}
		}
	}
	mysem_up(lock);
}
struct program_node * find_ip_port(double group_id){
	mysem_down(lock);
	struct group_node *current;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			mysem_up(lock);
			return current->program_list_head->next;
		}
	}
	mysem_up(lock);
	return(NULL);
}
//Select next program to run.Start from the first group in the group list
struct program_node *get_next_program_from_start(struct group_node** running_group){
	mysem_down(lock);
	struct program_node *current;
	struct group_node *current_group;
	int group_list_length=0,counter=0;
	
	//calculate group list length
	for(current_group=group_list_head->next;;current_group=current_group->next){
		if(current_group==group_list_head){break;}
		group_list_length++;
	}
	
	current_group=group_list_head->next;
	//search all groups for a not sleeping program
	while(1){
		counter++;
		if(counter==group_list_length){break;}
		for(current=current_group->program_list_head->next;;current=current->next){
			if(current==current_group->program_list_head){break;}
			if(current->sleep_bit==0  && current->send_bit==0 && current->receive_bit==0 && current->migrate_bit==0){
				*running_group=current_group;
				mysem_up(lock);
				return(current);
			}
		}
		current_group=current_group->next;
		//skip idle process
	}
	*running_group=group_list_head->prev;
	mysem_up(lock);
	return(group_list_head->prev->program_list_head->next);
}
//Select next program to run.Start from the program next to running_program
struct program_node *get_next_program(struct program_node * running_program,struct group_node** running_group){
	mysem_down(lock);
	struct program_node *current;
	struct group_node *current_group;
	int group_list_length=0,counter=0;
	
	//calculate group list length
	for(current_group=group_list_head->next;;current_group=current_group->next){
		if(current_group==group_list_head){break;}
		group_list_length++;
	}
	if((*running_group)!=group_list_head->prev){
		//search in the running group for a not sleeping program
		for(current=running_program->next;;current=current->next){
			if(current==(*running_group)->program_list_head){break;}
			if(current->sleep_bit==0  && current->send_bit==0 && current->receive_bit==0 && current->migrate_bit==0){
				mysem_up(lock);
				return(current);
			}
		}
		current_group=(*running_group)->next;
	}
	else{current_group=(*running_group)->next->next;}
	//search all groups for a not sleeping program
	while(1){
		counter++;
		if(current_group==group_list_head->prev && counter<group_list_length){
			current_group=current_group->next->next;
		}
		else if(counter==group_list_length){break;}
		else{
			for(current=current_group->program_list_head->next;;current=current->next){
				if(current==current_group->program_list_head){break;}
				if(current->sleep_bit==0 && current->send_bit==0 && current->receive_bit==0 && current->migrate_bit==0){
					*(running_group)=current_group;
					mysem_up(lock);
					return(current);
				}
			}
			if(counter==group_list_length){break;}
			current_group=current_group->next;
			//skip idle process
		}
	}
	
	*(running_group)=group_list_head->prev;
	mysem_up(lock);
	return(group_list_head->prev->program_list_head->next);
}
//Delete all programs which have delete_bit=1
int check_for_delete_programs(int program_id){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program,*next;
	int delete_running_program=0;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		for(current_program=current->program_list_head->next;;){
			if(current_program==current->program_list_head){break;}
			if(current_program->delete_bit==1){
				if(current_program->program_id==program_id){
					delete_running_program=1;
				}
				current->members--;
				next=current_program->next;
				delete_variable_list(current_program->variable_list_head);
				current_program->prev->next=current_program->next;
				current_program->next->prev=current_program->prev;
				free(current_program);
				current_program=next;
			}
			else{
				current_program=current_program->next;
			}
		}
		if(current->members==0){
			current->delete_bit=1;
		}
	}
	mysem_up(lock);
	return(delete_running_program);
}
//Delete all groups which have delete_bit=1
int check_for_delete_groups(double group_id){
	mysem_down(lock);
	struct group_node *current,*next;
	int delete_running_group=0;
	
	for(current=group_list_head->next;;){
		if(current==group_list_head){break;}
		if(current->delete_bit==1){
			if(compare_float(current->group_id,group_id)==1){
				delete_running_group=1;
			}
			next=current->next;
			delete_program_list(current->program_list_head);
			
			current->prev->next=current->next;
			current->next->prev=current->prev;
			free(current);
			current=next;
		}
		else{
			current=current->next;
		}
	}
	mysem_up(lock);
	return(delete_running_group);
}
//Copy basic state bytes to migrate struct
struct migrate_node *search_and_copy(double group_id,int program_id,char *migrate_ip,int migrate_port,int migrate_bit,struct migrate_node * migrate_struct){
	mysem_down(lock);
	struct group_node *current;
	int fd,bytes_returned=0,i=0,valid_command=0;
	char command[MAX_INPUT_SIZE];
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->program_id==program_id){
					
					current_program->migrate_port=migrate_port;
					strcpy(current_program->migrate_ip,migrate_ip);
					current_program->migrate_bit=migrate_bit;
					migrate_struct->file_size=0;
					
					migrate_struct->program_id=htonl(current_program->program_id);
					migrate_struct->group_id=current_program->group_id;
					migrate_struct->sleep_bit=htonl(current_program->sleep_bit);
					migrate_struct->send_bit=htonl(current_program->send_bit);
					migrate_struct->receive_bit=htonl(current_program->receive_bit);
					migrate_struct->sleep_time=htonl(current_program->sleep_time);
					migrate_struct->label_found=htonl(current_program->label_found);
					migrate_struct->program_counter=htonl(current_program->program_counter);
					migrate_struct->delete_bit=htonl(current_program->delete_bit);
					migrate_struct->home_port=htonl(current_program->home_port);
					strcpy(migrate_struct->home_ip,current_program->home_ip);
					strcpy(migrate_struct->filename,current_program->filename);
					
					if(current_program->receive_bit==1){
						//printf("program is blocked in receive function\n");
						strcpy(migrate_struct->command,current_program->command);
						//("command=%s\n",current_program->command);
						migrate_struct->command_position=ntohl(ftell(current_program->fd));
						//printf("migrate_struct->command_position=%d\n",ntohl(migrate_struct->command_position));
					}
					else{
						while(1){
							fgets(command,MAX_INPUT_SIZE,current_program->fd);
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
						command[strlen(command)-1]='\0';
						strcpy(migrate_struct->command,command);
						migrate_struct->command_position=ntohl(ftell(current_program->fd));
					}
					
					fd=open(current_program->filename,O_CREAT|O_RDWR,S_IRWXU);
					if(fd==-1){
						printf("error with open\n");
					}
					i=0;
					while(1) {
						bytes_returned=read(fd,&(migrate_struct->mycode[i]),1);
						migrate_struct->file_size+=bytes_returned;
						i++;
						if(bytes_returned==0) {break;}
					}
					migrate_struct->mycode[migrate_struct->file_size]='\0';
					
					migrate_struct->file_size=htonl(migrate_struct->file_size);
					mysem_up(lock);
					return(migrate_struct);
				}
			}
		}
	}
	mysem_up(lock);
	return(migrate_struct);
}
//Copy variable list to second step struct
struct second_step *search_and_copy_2nd_step(double group_id,int program_id,char *migrate_ip,int migrate_port,int migrate_bit,struct second_step * second_step_send){	
	
	mysem_down(lock);
	struct group_node *current;
	int counter=0,position=0;
	struct program_node *current_program;
	struct var_node * current_var;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->program_id==program_id){
					for(current_var=current_program->variable_list_head->next;;current_var=current_var->next){
						if(current_var==current_program->variable_list_head){break;}
						
						memcpy(&second_step_send->buffer[position],&current_var->type,sizeof(int));
						position+=sizeof(int);
						memcpy(&second_step_send->buffer[position],current_var->variable_name,20);
						position+=20;
						memcpy(&second_step_send->buffer[position],&current_var->int_variable,sizeof(int));
						position+=sizeof(int);
						memcpy(&second_step_send->buffer[position],current_var->string_variable,20);
                        position+=20;
						//printf("var[%d] %s with value %d\n",counter,current_var->variable_name,current_var->int_variable);
						counter++;
					}
					second_step_send->num_of_variables=htonl(counter);
					mysem_up(lock);
					return(second_step_send);
				}
			}
		}
	}
	mysem_up(lock);
	return(second_step_send);
}
//Copy message list to third step struct
struct third_step *search_and_copy_3nd_step(double group_id,int program_id,char *migrate_ip,int migrate_port,int migrate_bit,struct third_step * third_step_send){	
	
	mysem_down(lock);
	struct group_node *current;
	int counter=0,i=0;
	struct program_node *current_program;
	struct message_node *current_message;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->program_id==program_id){
					counter=0;
					for(current_message=current_program->message_list_head->next;;current_message=current_message->next){
						if(current_message==current_program->message_list_head){break;}
						
						third_step_send->migrate_message_table[counter].sender_id=htonl(current_message->sender_id);
						third_step_send->migrate_message_table[counter].receiver_id=htonl(current_message->receiver_id);
						third_step_send->migrate_message_table[counter].length=htonl(current_message->length);
						
						for(i=0;i<current_message->length;i++){
							strcpy(third_step_send->migrate_message_table[counter].array_of_variables[i],current_message->array_of_variables[i]);
						}
						third_step_send->migrate_message_table[counter].next=NULL;
						third_step_send->migrate_message_table[counter].prev=NULL;
						counter++;
						
					}
					third_step_send->num_of_messages=htonl(counter);
					mysem_up(lock);
					return(third_step_send);
				}
			}
		}
	}
	mysem_up(lock);
	return(third_step_send);
}
void update_migrate_program(int program_id,double group_id,int migration_port,char *migration_ip){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->program_id==program_id){
					strcpy(current_program->migrate_ip,migration_ip);
					current_program->migrate_port=migration_port;
					break;
				}
			}
		}
	}
	mysem_up(lock);
}
//compares if the float f1 is equal with f2 and returns 1 if true and 0 if false
int compare_float(float f1, float f2){
	float precision = 0.00001;
	if (((f1 - precision) < f2) && ((f1 + precision) > f2)){
		return 1;
	}
	else{return 0;}
}
struct group_node *group_exists(double group_id){
	mysem_down(lock);
	struct group_node *current;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			mysem_up(lock);
			return(current);
		}
	}
	mysem_up(lock);
	return(NULL);
}
struct program_node *program_exists(double group_id,int program_id){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){
					mysem_up(lock);
					return(NULL);
				}
				if(current_program->program_id==program_id){
					mysem_up(lock);
					return(current_program);
				}
			}
		}
	}
	mysem_up(lock);
	return(NULL);
}
void delete_program_after_migration(double group_id,int program_id){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			if(current->original==1){
				mysem_up(lock);
				return;
			}
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->program_id==program_id){
					current->members--;
					delete_variable_list(current_program->variable_list_head);
					current_program->prev->next=current_program->next;
					current_program->next->prev=current_program->prev;
					free(current_program);
					break;
				}
			}
			if(current->members==0){
				free(current->program_list_head);
				current->prev->next=current->next;
				current->next->prev=current->prev;
				free(current);
				mysem_up(lock);
				return;
			}
		}
	}
	mysem_up(lock);
}
void delete_program_after_ret(double group_id,int program_id){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(compare_float(current->group_id,group_id)==1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->program_id==program_id){
					current->members--;
					delete_variable_list(current_program->variable_list_head);
					current_program->prev->next=current_program->next;
					current_program->next->prev=current_program->prev;
					free(current_program);
					break;
				}
			}
			if(current->members==0){
				free(current->program_list_head);
				current->prev->next=current->next;
				current->next->prev=current->prev;
				free(current);
				mysem_up(lock);
				return;
			}
		}
	}
	mysem_up(lock);
}
int find_total_processes(){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	int total_processes=0;
		
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		for(current_program=current->program_list_head->next;;current_program=current_program->next){
			if(current_program==current->program_list_head){break;}
			if(current_program->delete_bit==0 && current_program->migrate_bit==0){total_processes++;}
		}
	}
	mysem_up(lock);
	return(total_processes);
}
void find_a_process_to_migrate(double *group_id,int *program_id){
	mysem_down(lock);
	struct group_node *current;
	struct program_node *current_program;
	
	for(current=group_list_head->next;;current=current->next){
		if(current==group_list_head){break;}
		if(current->group_id!=1){
			for(current_program=current->program_list_head->next;;current_program=current_program->next){
				if(current_program==current->program_list_head){break;}
				if(current_program->delete_bit==0 && current_program->migrate_bit==0){
					*program_id=current_program->program_id;
					*group_id=current->group_id;
					mysem_up(lock);
					return;
				}
			}
		}
	}
	mysem_up(lock);
}
