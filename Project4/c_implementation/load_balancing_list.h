struct balance_node *balance_head;
struct balance_node{
	char home_ip[200];
	int home_port;
    int total_processes;
	struct balance_node *next;
	struct balance_node *prev;
};

void balance_list_init(){
	balance_head=(struct balance_node *)malloc(sizeof(struct balance_node));
	balance_head->next=balance_head;
	balance_head->prev=balance_head;
}
//Delete balanxe lsit
void balance_list_delete(){
	
	struct balance_node *current,*temp;
	
	for(current=balance_head->next;;){
		if(current==balance_head){break;}
		temp=current->next;
		current->prev->next=current->next;
		current->next->prev=current->prev;
		free(current);
		current=temp;
	}
}
//Add a node to balance list
void balance_list_add(char *ip,int port,int total_processes){
	
	struct balance_node *new_node=(struct balance_node *)malloc(sizeof(struct balance_node));
	
	strcpy(new_node->home_ip,ip);
	new_node->home_port=port;
	new_node->total_processes=total_processes;
	
	new_node->next=balance_head->next;
	balance_head->next->prev=new_node;
	balance_head->next=new_node;
	new_node->prev=balance_head;
}
//Find the average value of total total_processes
int find_avg_from_balance_list(){
	
	struct balance_node *current;
	int length=0,avg=0;
	
	for(current=balance_head->next;;current=current->next){
		if(current==balance_head){break;}
		length++;
		avg+=current->total_processes;
	}
	return(avg/length);
}
//Find the node with the smaller total_processes value
struct balance_node *find_min(){
	
	struct balance_node *current,*min;
	int min_val=1000;
	
	for(current=balance_head->next;;current=current->next){
		if(current==balance_head){break;}
		if(current->total_processes<min_val){
			min=current;
			min_val=current->total_processes;
		}
	}
	return(min);
}
//Find the node with the higher total_processes value
struct balance_node *find_max(){
	
	struct balance_node *current,*max;
	int max_val=-1;
	
	for(current=balance_head->next;;current=current->next){
		if(current==balance_head){break;}
		if(current->total_processes>max_val){
			max=current;
			max_val=current->total_processes;
		}
	}
	return(max);
}
int find_gap(int avg_val,int gap){
	
	struct balance_node *current;
	
	for(current=balance_head->next;;current=current->next){
		if(current==balance_head){break;}
		if(abs(current->total_processes-avg_val)>=gap){
			return(0);
		}
	}
	return(1);

}
struct balance_node * find_min_for_shutdown(int my_port,char *myip){
	
	struct balance_node *current;
	
	for(current=balance_head->next;;current=current->next){
		if(current==balance_head){break;}
		if(current->home_port!=my_port || strcmp(current->home_ip,myip)!=0){
			return(current);
		}
	}
	return(current);
}
