#include <string.h>
#include <limits.h>
#include "semaphores.h"
#include "mink_list.h"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define MAX_MESSAGE_LENGTH 100
#define EMPTY_BODY -10
#define MESSAGE_EXISTS -11
#define MESSAGE_DOES_NOT_EXIST -12
#define NODE_WITHOUT_SEQNUM_EXISTS -13
#define NODE_WITHOUT_SEQNUM_NOT_EXISTS -14
#define NODE_WITH_CORRECT_SEQNUM_EXISTS -15
#define MESSAGE_ACKS_MISSING -16
#define SEQUENCE_ACKS_MISSING -17
#define MESSAGE_ACKS_OK -18
#define SEQUENCE_ACKS_OK -19
#define RESET_FIRST_FLAG -20
#define MESSAGE -21
#define SEQUENCE -22
struct message_node{
	char payload[MAX_MESSAGE_LENGTH];
	int sequence_number;
	int sender_pid;
	int deliver;
	int deliver_number;
	int sequencer_send_id;
	int msg_acks;
	int seq_acks;
	long int start_message_send;
	long int start_sequence_send;
	int message_retransmit_end;
	int sequence_retransmit_end;
	struct message_node* next;
	struct message_node* prev;
};
struct message_node *rm_head;
int lock;
void reliable_multicast_list_init(){
	lock=mysem_create(lock,1);
	rm_head=(struct message_node *)malloc(sizeof(struct message_node));
	rm_head->next=rm_head;
	rm_head->prev=rm_head;
}
//All functions have semaphores because they are accessed from a lot of threads
//Add a message int the message list
void reliable_multicast_list_add(char *payload,int seqid,int sendpid,int sequencer_send__id){
	mysem_down(lock);
	struct message_node *new_node=(struct message_node *)malloc(sizeof(struct message_node));
	
	new_node->sequence_number=seqid;
	new_node->sender_pid=sendpid;
	new_node->msg_acks=0;
	new_node->seq_acks=0;
	strcpy(new_node->payload,payload);
	new_node->deliver=0;
	new_node->sequencer_send_id=sequencer_send__id;
	new_node->message_retransmit_end=0;
	new_node->sequence_retransmit_end=0;
	new_node->deliver_number=0;
	new_node->next=rm_head->next;
	rm_head->next->prev=new_node;
	rm_head->next=new_node;
	new_node->prev=rm_head;
	mysem_up(lock);
}
//Add a message with empty body in the message list
void add_node_without_body(int sendpid,int seqid,int delivery_num,int sequencer_send__id){
	mysem_down(lock);
	struct message_node *new_node=(struct message_node *)malloc(sizeof(struct message_node));
	
	new_node->sequence_number=seqid;
	new_node->sender_pid=sendpid;
	bzero(new_node->payload,MAX_MESSAGE_LENGTH);
	new_node->deliver=0;
	new_node->msg_acks=0;
	new_node->seq_acks=0;
	new_node->sequencer_send_id=sequencer_send__id;
	new_node->message_retransmit_end=0;
	new_node->sequence_retransmit_end=0;
	new_node->deliver_number=delivery_num;
	new_node->next=rm_head->next;
	rm_head->next->prev=new_node;
	rm_head->next=new_node;
	new_node->prev=rm_head;
	mysem_up(lock);
}
//Search in the message list for a pid.k message.Return MESSAGE_EXISTS or MESSAGE_DOES_NOT_EXIST or EMPTY_BODY
int reliable_multicast_list_search(int pid,int k){
	mysem_down(lock);
	struct message_node *current;
	char temp_buf[MAX_MESSAGE_LENGTH];
	bzero(temp_buf,MAX_MESSAGE_LENGTH);
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==(pid+k)){
			if(strcmp(current->payload,temp_buf)==0){
				mysem_up(lock);
				return EMPTY_BODY;
			}
			else{
				mysem_up(lock);
				return(MESSAGE_EXISTS);
			}
		}
		if(current->next==rm_head){
			mysem_up(lock);
			return(MESSAGE_DOES_NOT_EXIST);
		}
	}
	return(MESSAGE_DOES_NOT_EXIST);
}
//Search in the message list for a pid.k message.Return NODE_WITHOUT_SEQNUM_EXISTS or NODE_WITH_CORRECT_SEQNUM_EXISTS or NODE_WITHOUT_SEQNUM_NOT_EXISTS
int search_node_without_seq_num(int pid,int k){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==(pid+k) && current->deliver_number==0){
			mysem_up(lock);
			return(NODE_WITHOUT_SEQNUM_EXISTS);
		}
		else if((current->sender_pid + current->sequence_number)==(pid+k) && current->deliver_number!=0){
			mysem_up(lock);
			return(NODE_WITH_CORRECT_SEQNUM_EXISTS);
		}
		if(current->next==rm_head){
			mysem_up(lock);
			return(NODE_WITHOUT_SEQNUM_NOT_EXISTS);
		}
	}
	return(NODE_WITHOUT_SEQNUM_NOT_EXISTS);
}
//Update payload in a pid.k  message inside message list 
int copy_message_body(int pid,int k,char *body){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==(pid+k)){
			strcpy(current->payload,body);
			current->deliver=1;
			mysem_up(lock);
			return(current->deliver_number);
		}
		if(current->next==rm_head){
			mysem_up(lock);
			return(1);
		}
	}
	return(1);
}
//Update sequence number  in a pid.k  message inside message list 
void copy_seqnum(int pid,int k,int delivery_num,int sequencer_send__id){
	mysem_down(lock);
	struct message_node *current;
		
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==(pid+k)){
			current->deliver=1;
			current->sequencer_send_id=sequencer_send__id;
			current->deliver_number=delivery_num;
			break;
		}
		if(current->next==rm_head){break;}
	}
	mysem_up(lock);
}
//Return payload from a pid.k message 
int return_message_to_api(char *message,int order){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->deliver_number==order && current->deliver==1)){
			strcpy(message,current->payload);
			mysem_up(lock);
			return(0);
		}
		if(current->next==rm_head){
			mysem_up(lock);
			return(1);
		}
	}
	return(1);
}
//Garbage collect all delivered messages
void garbage_collect(int receive_max,struct node *min_head){
	mysem_down(lock);
	int temp_sender_pid,min_k;
	struct message_node *current,*temp;
	struct node *current_min;
	
	current=rm_head->prev;
	while(current!=rm_head){
		for(current_min=min_head->next;;current_min=current_min->next){
			if(current_min->sender_pid==current->sender_pid){
				temp_sender_pid=current_min->sender_pid;
				min_k=current_min->min_k;
				break;
			}
			if(current_min->next==min_head){break;}
		}
		if(current->sender_pid==temp_sender_pid && current->deliver_number < receive_max && current->message_retransmit_end==1 && current->sequence_retransmit_end==1 && current->sequence_number <= min_k){
			temp=current->prev;
			current->prev->next=current->next;
			current->next->prev=current->prev;
			free(current);
			
			current=temp;
		}
		else{current=current->prev;}
	}
	mysem_up(lock);
}
//Update message acks counter
void update_msg_ack_counter(int pid,int k,int members){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid==pid && current->sequence_number==k)){
			current->msg_acks++;
			if(current->msg_acks==members){current->message_retransmit_end=1;}
			mysem_up(lock);
			break;
		}
		if(current->next==rm_head){
			mysem_up(lock);
			break;
		}
	}
}
//Update sequence message acks counter
void update_seq_ack_counter(int pid,int k,int members){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid==pid && current->sequence_number==k)){
			current->seq_acks++;
			if(current->seq_acks==members){current->sequence_retransmit_end=1;}
			mysem_up(lock);
			break;
		}
		if(current->next==rm_head){
			mysem_up(lock);
			break;
		}
	}
}
//Print all messages from message list.Print also their logistics
void print_list(){
	mysem_down(lock);
	struct message_node *current;
	
	printf("********************************************************************START********************************************************************\n");
	for(current=rm_head->next;;current=current->next){
		printf("%d.%d ,sequence number=%d,start_msg_time =%ld ,start_seq_time=%ld,msg_args_flag=%d,seq_args_flag=%d,msg_acks=%d,seq_acks=%d\n",current->sender_pid,current->sequence_number,current->deliver_number,current->start_message_send,current->start_sequence_send,current->message_retransmit_end,current->sequence_retransmit_end,current->msg_acks,current->seq_acks);
		if(current->next==rm_head){
			break;
		}
	}
	printf("*********************************************************************END*********************************************************************\n");
	mysem_up(lock);
}
//Find minimum send time of all messages 
long int find_min_start_time(int *retransmit_pid,int *retransmit_k,int *type){
	mysem_down(lock);
	struct message_node *current;
	char temp[MAX_MESSAGE_LENGTH];
	bzero(temp,MAX_MESSAGE_LENGTH);
	long int min_time=LONG_MAX;
	int length=0,list_is_ok=0;
	
	for(current=rm_head->prev;;current=current->prev){
		length++;
		if(current->prev==rm_head){
			break;
		}
	}
	for(current=rm_head->prev;;current=current->prev){
		//Check only valid messages,those with missing message or requence-message acks.Message must be not empty and sequnce_number must be different from zero
		if(current->message_retransmit_end==1 && current->sequence_retransmit_end==1){
			list_is_ok++;
		}
		if(current->prev==rm_head){
			if(list_is_ok==length || length==1){
				//print_k_list();
				//printf("Ok nodes=%d,List length=%d\n",list_is_ok,length);
				mysem_up(lock);
				return(RESET_FIRST_FLAG);
			}
			break;
		}
	}
	for(current=rm_head->prev;;current=current->prev){
		//Check only valid messages,those with missing message or requence-message acks.Message must be not empty and sequnce_number must be different from zero
		if(current->message_retransmit_end==0 || current->sequence_retransmit_end==0){
			if(current->start_message_send<min_time && current->message_retransmit_end==0 && strcmp(current->payload,temp)!=0){
				min_time=current->start_message_send;
				*retransmit_pid=current->sender_pid;
				*retransmit_k=current->sequence_number;
				*type=MESSAGE;
			}
			if(current->start_sequence_send<min_time && current->sequence_retransmit_end==0 && current->deliver_number!=0 ){
				min_time=current->start_sequence_send;
				*retransmit_pid=current->sender_pid;
				*retransmit_k=current->sequence_number;
				*type=SEQUENCE;
			}
		}
		if(current->prev==rm_head){
			mysem_up(lock);
			if(min_time==LONG_MAX){return(RESET_FIRST_FLAG);}
			return(min_time);
		}
	}
}
//Update message send time for a pid.k message
void set_message_start_time(int pid,int k,long int start_time){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==pid+k){
			current->start_message_send=start_time;
			mysem_up(lock);
			break;
		}
		if(current->next==rm_head){
			mysem_up(lock);
			break;
		}
	}
}
//Update sequence message send time for a pid.k sequence message
void set_sequence_start_time(int pid,int k,long int start_time){
	mysem_down(lock);
	struct message_node *current;
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==pid+k){
			current->start_sequence_send=start_time;
			mysem_up(lock);
			break;
		}
		if(current->next==rm_head){
			mysem_up(lock);
			break;
		}
	}
}
//Find if a pid.k message has received all message acks and all sequence message acks
//message_retransmit_end means that this node has received all acks for message sends
//sequence_retransmit_end means that this node has received all acks for sequence message sends
int check_acks(int pid,int k,int members,int retrtype){
	mysem_down(lock);
	struct message_node *current;
	char temp[MAX_MESSAGE_LENGTH];
	bzero(temp,MAX_MESSAGE_LENGTH);
	
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==pid+k){
			if(retrtype==MESSAGE){
				if(current->message_retransmit_end==0){
					mysem_up(lock);
					return(MESSAGE_ACKS_MISSING);
				}
				else{
					mysem_up(lock);
					return(MESSAGE_ACKS_OK);
				}
			}
			if(retrtype==SEQUENCE){
				if(current->sequence_retransmit_end==0){
					mysem_up(lock);
					return(SEQUENCE_ACKS_MISSING);
				}
				else{
					mysem_up(lock);
					return(SEQUENCE_ACKS_OK);
				}
			}
		}
		if(current->next==rm_head){
			mysem_up(lock);
			break;
		}
	}
	return(0);
}
//Find a pid.k message in the message list and copy all message struct elements 
void resend_from_handler(int pid,int k,int *sender_pid,int *sequence_number,char *payload,int *delivery_number,int *sequencer_id,int resendtype){
	mysem_down(lock);
	struct message_node *current;
	for(current=rm_head->next;;current=current->next){
		if((current->sender_pid + current->sequence_number)==pid+k){
			*sender_pid=current->sender_pid;
			*sequence_number=current->sequence_number;
			strcpy(payload,current->payload);
			*delivery_number=current->deliver_number;
			*sequencer_id=current->sequencer_send_id;
			if(resendtype==1){current->msg_acks=0;}
			else{current->seq_acks=0;}
			mysem_up(lock);
			break;
		}
		if(current->next==rm_head){
			mysem_up(lock);
			break;
		}
	}
}
void insertionSort(int *arr,int n){ 
	int i,key,j; 
	for (i = 1; i < n; i++) { 
		key = arr[i]; 
		j = i - 1; 

		/* Move elements of arr[0..i-1], that are 
			greater than key, to one position ahead 
			of their current position */
		while (j >= 0 && arr[j] > key) { 
			arr[j + 1] = arr[j]; 
			j = j - 1; 
		} 
		arr[j + 1] = key; 
	} 
} 
int update_min_k_from_request_list(int pid,int min_k){
	mysem_down(lock);
	char temp[MAX_MESSAGE_LENGTH];
	bzero(temp,MAX_MESSAGE_LENGTH);
	int array_length=0,j;
	struct message_node *current;
	int *k_buffer=(int *)malloc(sizeof(int));
	
	
	for(current=rm_head->next;;current=current->next){
		if(current->sender_pid==pid && strcmp(temp,current->payload)!=0 && current->sequence_number > min_k){
			k_buffer=realloc(k_buffer,(array_length+1)*sizeof(int));
			k_buffer[array_length]=current->sequence_number;
			array_length++;
		}
		if(current->next==rm_head){break;}
	}
	insertionSort(k_buffer,array_length);
	for(j=0;j<array_length;j++){
		if(k_buffer[j]==min_k+1){min_k++;}
		else{break;}
	}
	mysem_up(lock);
	return(min_k);
}
