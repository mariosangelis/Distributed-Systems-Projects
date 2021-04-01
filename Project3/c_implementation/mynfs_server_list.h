struct nfs_server_node *server_head;
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define GARBAGE_COLLECT_TIME 20
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
struct nfs_server_node{
	char file_name[100];
	int fd_OS;
	int fd_client;
	int pos;
	long int active_time;
	int tmod;
	struct nfs_server_node* next;
	struct nfs_server_node* prev;
};
//Initialize nfs_server list
void nfs_server_list_init(){
	server_head=(struct nfs_server_node *)malloc(sizeof(struct nfs_server_node));
	server_head->next=server_head;
	server_head->prev=server_head;
}
//Add a node in the nfs_server list
struct nfs_server_node * nfs_server_list_add(char *file_name,int fd_OS,int fd_client,long int set_time){
	struct nfs_server_node *new_node=(struct nfs_server_node *)malloc(sizeof(struct nfs_server_node));
	new_node->fd_OS=fd_OS;
	new_node->fd_client=fd_client;
	new_node->pos=0;
	new_node->active_time=set_time;
	new_node->tmod=0;
	strcpy(new_node->file_name,file_name);
	
	new_node->next=server_head->next;
	server_head->next->prev=new_node;
	server_head->next=new_node;
	new_node->prev=server_head;
	return new_node;
}
//Find tmod,operating system dile descriptor for an already known client file descriptor
void nfs_server_list_search(int fd,int *send_fid,int *tmod){
	struct nfs_server_node *current;
	
	for(current=server_head->next;;current=current->next){
		if(current==server_head){
			*send_fid=-1;
			break;
		}
		if(current->fd_client==fd){
			*send_fid=current->fd_OS;
			*tmod=current->tmod;
			break;
		}
	}
}
//Update mod time due to a new writing in the file
int update_tmod(int fd){
	struct nfs_server_node *current;
	
	for(current=server_head->next;;current=current->next){
		if(current==server_head){break;}
		if(current->fd_client==fd){
			current->tmod++;
			break;
		}
	}
	return(current->tmod);
}
//Update active time of a node
void update_active_time(int fd_os,long int set_time){
	struct nfs_server_node *current;
	
	for(current=server_head->next;;current=current->next){
        if(current==server_head){break;}
		if(current->fd_OS==fd_os){
			current->active_time=set_time;
			break;
		}
	}
}
//Delete the nodes of which the active_time is smaller than the GARBAGE_COLLECT_TIME
void garbage_collect(long int current_time){
	
	struct nfs_server_node *current,*temp;
	
	for(current=server_head->next;;current=current->next){
		if(current==server_head){break;}
		if((current_time-current->active_time)>=GARBAGE_COLLECT_TIME){
			printf("Delete 1 node from server list...\n");
			temp=current->prev;
			current->prev->next=current->next;
			current->next->prev=current->prev;
			free(current);
			
			current=temp;
		}
	}
}
//Find minimum active time
long int find_next_retr_time(){
	long int min=1000000;
	struct nfs_server_node *current;
	
	for(current=server_head->next;;current=current->next){
		if(current==server_head){break;}
		if(current->active_time<min){
			min=current->active_time;
		}
	}
	if(min==1000000){return(-1);}
	return(min);
}
//Print cache
void print_list(){
	
	struct nfs_server_node *current;
	
	printf(ANSI_COLOR_BLUE"*********************List start*********************\n"ANSI_COLOR_RESET);
	for(current=server_head->next;;current=current->next){
		if(current==server_head){break;}
		printf(ANSI_COLOR_GREEN"fd_client=%d,fd_OS=%d,active_time=%ld\n"ANSI_COLOR_RESET,current->fd_client,current->fd_OS,current->active_time);
	}
	printf(ANSI_COLOR_BLUE"*********************List end*********************\n"ANSI_COLOR_RESET);
}
