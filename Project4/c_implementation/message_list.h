#include "variable_list.h"
struct message_node{
	int sender_id;
	int receiver_id;
	int length;
	char array_of_variables[40][20];
	struct message_node *next;
	struct message_node *prev;
};

struct message_node * message_list_init(struct message_node *head){
	head=(struct message_node *)malloc(sizeof(struct message_node));
	head->next=head;
	head->prev=head;
	return(head);
}
struct message_node * message_list_delete(struct message_node *head,int sender_id){
	
	struct message_node *current;
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(sender_id==current->sender_id){
			current->prev->next=current->next;
			current->next->prev=current->prev;
			free(current);
			break;
		}
	}
	return(head);
}
struct message_node * message_list_add(struct message_node *head,int sender_id,int receiver_id,char array_of_token[40][20],int length){
	
	struct message_node *new_node=(struct message_node *)malloc(sizeof(struct message_node));
	int i=1,counter=0;
	
	new_node->sender_id=sender_id;
	new_node->receiver_id=receiver_id;
	new_node->length=length-1;
	
	while(i<length){
		strcpy(new_node->array_of_variables[counter],array_of_token[i]);
		i++;
		counter++;
	}
	new_node->next=head->next;
	head->next->prev=new_node;
	head->next=new_node;
	new_node->prev=head;
	return(head);
}
struct message_node * message_list_add_after_migration(struct message_node *head,int sender_id,int receiver_id,char array_of_token[40][20],int length){
	
	struct message_node *new_node=(struct message_node *)malloc(sizeof(struct message_node));
	int i=0;
	
	new_node->sender_id=sender_id;
	new_node->receiver_id=receiver_id;
	new_node->length=length;
	while(i<length){
		strcpy(new_node->array_of_variables[i],array_of_token[i]);
		i++;
	}
	new_node->next=head->next;
	head->next->prev=new_node;
	head->next=new_node;
	new_node->prev=head;
	return(head);
}

