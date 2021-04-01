/*This is the list which includes replies in the client side*/
struct reply_node{
    char payload[1024];
	int reqid;
	struct reply_node* next;
	struct reply_node* prev;
};
struct reply_node* reply_head;
int reply_lock;
struct reply_node * reply_init_list(){
    reply_lock=mysem_create(reply_lock,1);
	reply_head=(struct reply_node *)malloc(sizeof(struct reply_node));
	reply_head->next=reply_head;
	reply_head->prev=reply_head;
	return reply_head;
}
void reply_list_add(char payload[1024],int id){
    mysem_down(reply_lock);
	struct reply_node *new_node=(struct reply_node *)malloc(sizeof(struct reply_node));
	new_node->reqid=id;
    memcpy(new_node->payload,payload,1024);
	new_node->next=reply_head->next;
	reply_head->next->prev=new_node;
	reply_head->next=new_node;
	new_node->prev=reply_head;
    mysem_up(reply_lock);
}
void reply_list_delete(int reqid){
    mysem_down(reply_lock);
	struct reply_node *current;
	for(current=reply_head->next;;current=current->next){
		if(current->reqid==reqid){
			current->next->prev=current->prev;
			current->prev->next=current->next;
			free(current);
            mysem_up(reply_lock);
			break;
		}
	}
}
struct reply_node *reply_list_search(int reqid){
    mysem_down(reply_lock);
	struct reply_node *current;
	for(current=reply_head->next;;current=current->next){
		if(current->reqid==reqid){
            mysem_up(reply_lock);
			return current;
		}
		if(current->next==reply_head){
            mysem_up(reply_lock);
            return (NULL);
        }
	}
	mysem_up(reply_lock);
	return(NULL);
}
