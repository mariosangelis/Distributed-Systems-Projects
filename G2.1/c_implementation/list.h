/*This is the list which includes requests in the server side*/
struct node{
	char payload[1024];
	char ack_addr[100];
	int ack_port;
	int reqid;
	int client_unique_id;
	struct node* next;
	struct node* prev;
};
struct node* head;
struct node * init_list(){
	head=(struct node *)malloc(sizeof(struct node));
	head->next=head;
	head->prev=head;
	return head;
}
void list_add(char *payload,int id,int client_unique_id,char *ack_addr,int ack_port){
	struct node *new_node=(struct node *)malloc(sizeof(struct node));
	new_node->reqid=id;
	new_node->client_unique_id=client_unique_id;
	memcpy(new_node->payload,payload,1024);
	new_node->ack_port=ack_port;
	strcpy(new_node->ack_addr,ack_addr);

	new_node->next=head->next;
	head->next->prev=new_node;
	head->next=new_node;
	new_node->prev=head;
}
void list_search(int reqid,char *ack_addr,int *ack_port){
	
	struct node *current;
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(current->reqid==reqid){
			*ack_port=current->ack_port;
			strcpy(ack_addr,current->ack_addr);
			break;
		}
	}
	
}
void list_pop(){
	struct node *remove_node;
	head->prev->prev->next=head;
	remove_node=head->prev;
	head->prev=head->prev->prev;
	free(remove_node);
}
void list_clear(){
	struct node *current,*temp;
	for(current=head->next;;){
		if(current==head){break;}
		current->prev->next=current->next;
		current->next->prev=current->prev;
		temp=current->next;
		free(current);
		current=temp;
	}
}
int list_capacity(){
	int capacity=0;
	struct node *current;
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		capacity++;
		if(current->next==head){break;}
	}
	return(capacity);
}
