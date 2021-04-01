/*This is the list which includes requests in the client side,receiver has received a reply for all requests marked with flag=1*/
struct resend_node{
	int reqid;
	int resend_flag;
	struct resend_node* next;
	struct resend_node* prev;
};
struct resend_node* resend_head;
int resend_lock;
struct resend_node * resend_init_list(){
	resend_lock=mysem_create(resend_lock,1);
	resend_head=(struct resend_node *)malloc(sizeof(struct resend_node));
	resend_head->next=resend_head;
	resend_head->prev=resend_head;
	return resend_head;
}
void resend_list_add(int flag,int id){
	mysem_down(resend_lock);
	struct resend_node *new_node=(struct resend_node *)malloc(sizeof(struct resend_node));
	new_node->reqid=id;
	new_node->resend_flag=flag;
	new_node->next=resend_head->next;
	resend_head->next->prev=new_node;
	resend_head->next=new_node;
	new_node->prev=resend_head;
	mysem_up(resend_lock);
}
void resend_list_delete(int reqid){
	mysem_down(resend_lock);
	struct resend_node *current;
	for(current=resend_head->next;;current=current->next){
		if(current->reqid==reqid){
			current->next->prev=current->prev;
			current->prev->next=current->next;
			free(current);
			break;
		}
	}
	mysem_up(resend_lock);
}
int resend_list_search(int reqid){
	mysem_down(resend_lock);
	struct resend_node *current;
	for(current=resend_head->next;;current=current->next){
		if(current->reqid==reqid){
			mysem_up(resend_lock);
			return current->resend_flag;
		}
		if(current->next==resend_head){break;}
	}
	mysem_up(resend_lock);
	return(-1);
}
void setflag(int reqid){
	mysem_down(resend_lock);
	struct resend_node *current;
	for(current=resend_head->next;;){
		if(current->reqid==reqid){
			current->resend_flag=1;
            break;
		}
		if(current->next==resend_head){
			break;
		}
		else{current=current->next;}
	}
	mysem_up(resend_lock);
}
