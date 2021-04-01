struct node *min_k_head;
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define UPDATE_K -30
#define K_IS_BIGGER_NOT_UPDATE -31
#define K_IS_SMALLER_NOT_UPDATE -32
struct node{
	int sender_pid;
	int min_k;
	struct node *next;
	struct node *prev;
};
void min_k_list_init(){
	min_k_head=(struct node *)malloc(sizeof(struct node));
	min_k_head->sender_pid=-1;
	min_k_head->next=min_k_head;
	min_k_head->prev=min_k_head;
}
struct node * min_k_list_add(int pid){
	struct node *current;
	
	for(current=min_k_head->next;;current=current->next){
		if(current->sender_pid==pid){return(NULL);}
		if(current->next==min_k_head){break;}
	}
	struct node *new_node=(struct node *)malloc(sizeof(struct node));
	new_node->sender_pid=pid;
	new_node->min_k=0;
	new_node->next=min_k_head->next;
	min_k_head->next->prev=new_node;
	min_k_head->next=new_node;
	new_node->prev=min_k_head;
	
	
	for(current=min_k_head->next;;current=current->next){
		if(current->next==min_k_head){break;}
	}
	
	return new_node;
}
void min_k_delete(){
	
	struct node *current;
	for(current=min_k_head->next;;current=current->next){
		current->min_k=0;
		if(current->next==min_k_head){break;}
	}
}
int update_min_k(int pid,int k){
	
	struct node *current;
	
	for(current=min_k_head->next;;current=current->next){
		if(current->sender_pid == pid){
			if(current->min_k+1 ==k){
				current->min_k++;
				return(UPDATE_K);
			}
			else if(k>current->min_k){
				return(K_IS_BIGGER_NOT_UPDATE);
			}
			else{
				return(K_IS_SMALLER_NOT_UPDATE);
			}
		}
	}
	return(0);
}
void copy_new_k(int pid,int new_k){
	struct node *current;
	for(current=min_k_head->next;;current=current->next){
		if(current->sender_pid == pid){
			current->min_k=new_k;
			break;
		}
	}
}
void print_k_list(){
	
	struct node *current;
	for(current=min_k_head->next;;current=current->next){
		printf("pid:%d->k:%d\n",current->sender_pid,current->min_k);
		if(current->next==min_k_head){break;}
	}
	
}
