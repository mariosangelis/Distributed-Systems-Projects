/*This is the list used by the load-balancer*/
struct balance_node{
	int id;
	int capacity;
    char ack_addr[100];
    int ack_port;
	struct balance_node* next;
	struct balance_node* prev;
};
void balance_init_list(struct balance_node** head){
	*head=(struct balance_node *)malloc(sizeof(struct balance_node));
	(*head)->next=*head;
	(*head)->prev=*head;
}
void balance_list_add(struct balance_node** head,int id,int size,char *ack_addr,int ack_port){
	struct balance_node *new_node=(struct balance_node *)malloc(sizeof(struct balance_node));
	new_node->id=id;
	new_node->capacity=size;
	new_node->next=(*head)->next;
	new_node->ack_port=ack_port;
	strcpy(new_node->ack_addr,ack_addr);
	(*head)->next->prev=new_node;
	(*head)->next=new_node;
	new_node->prev=*head;
}
void balance_list_delete(struct balance_node** head){
	struct balance_node *current,*temp;
	for(current=(*head)->next;;){
		temp=current->next;
		free(current);
		current=temp;
		if(current==*head){
			free(*head);
			break;
		}
	}
}
int balance_list_search(struct balance_node** head,char *ack_addr,int *ack_port){
	struct balance_node *current;
	int min=200000,id;
	for(current=(*head)->next;;current=current->next){
		if(current->capacity<=min){
			min=current->capacity;
			id=current->id;
			strcpy(ack_addr,current->ack_addr);
			*ack_port=current->ack_port;
		}
		if(current->next==*head){return id;}
	}
	return(0);
}
