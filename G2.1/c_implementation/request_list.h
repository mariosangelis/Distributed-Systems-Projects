typedef struct noderequest {
	int request_id;
    int delete_flag;
	struct noderequest *next;
	struct noderequest *prev;
} RequestNode;

RequestNode *init_req_list(RequestNode *head) {

	head=(RequestNode*)malloc(sizeof(RequestNode));
	if(head==NULL) {
		fprintf(stderr,"Problem with memory allocation.\n");
		exit(1);
	}
	head->next=head;
	head->prev=head;
	return(head);
}
void add_in_request_list(RequestNode *head,int id) {
	RequestNode *new_node;

	new_node=(RequestNode*)malloc(sizeof(RequestNode));
	if(new_node==NULL) {
		fprintf(stderr,"Problem with memory allocation.\n");
		exit(1);
	}
	new_node->request_id=id;
	new_node->delete_flag=0;
	new_node->next=head;
	new_node->prev=head->prev;
	head->prev->next=new_node;
	head->prev=new_node;
}
int take_request(RequestNode *head,int position){
    int counter=0;
	RequestNode *node;
    for(node=head->next;;node=node->next){
		if(counter==position){return node->request_id;}
		counter++;
	}
	return(0);
}
void delete_request(RequestNode *head){
	RequestNode *node,*temp;
	for(node=head->next;;){
		if(node->delete_flag==1){
			node->prev->next=node->next;
			node->next->prev=node->prev;
			temp=node->next;
			free(node);
			node=temp;
			if(node==head){break;}
			
		}
		else{
			if(node->next==head) {
				break;
			}
			else{node=node->next;}
		}
	}
}
int get_reqlist_length(RequestNode *head) {
	int length=0;
	RequestNode *node;

	for(node=head->next;;node=node->next){
		length++;
		if(node->next==head) {
			break;
		}
	}
	return(length);
}
void set_delete_flag(RequestNode *head,int reqid){
    RequestNode *node;
    for(node=head->next;;node=node->next){
		if(node->request_id==reqid) {
			node->delete_flag=1;
			break;
		}
	}
}
