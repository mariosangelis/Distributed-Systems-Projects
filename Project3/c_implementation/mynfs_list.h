struct nfs_node *head;
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
struct nfs_node{
	char file_name[100];
	int flags;
	int fd_client;
	int fd_apl;
	int pos;
	int file_size;
	int tmod;
	long int tcheck;
	struct nfs_node* next;
	struct nfs_node* prev;
};
//Initialize nfs_client list
void nfs_list_init(){
	head=(struct nfs_node *)malloc(sizeof(struct nfs_node));
	head->next=head;
	head->prev=head;
}
void print_list(){
	
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		printf("fd_client=%d,fd_apl=%d\n",current->fd_client,current->fd_apl);
	}
}
//Add a node in the nfs_client list
struct nfs_node * nfs_list_add(char *file_name,int fd_apl,int fd_client,int tmod,long int tcheck,int size,int flags){
	struct nfs_node *new_node=(struct nfs_node *)malloc(sizeof(struct nfs_node));
	new_node->fd_apl=fd_apl;
	new_node->fd_client=fd_client;
	new_node->pos=0;
	new_node->tmod=tmod;
	new_node->flags=flags;
	new_node->file_size=size;
	new_node->tcheck=tcheck;
	strcpy(new_node->file_name,file_name);
	
	new_node->next=head->next;
	head->next->prev=new_node;
	head->next=new_node;
	new_node->prev=head;
	return new_node;
}
//Find tmod,tcheck,position,client file descriptor for an already known application file descriptor
void nfs_list_search(int fd,int *send_fid,int *pos,char *filename,long int *tcheck,int *tmod,int *filesize,int *flags){
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){
			*send_fid=-1;
			break;
		}
		if(current->fd_apl==fd){
			*send_fid=current->fd_client;
			*pos=current->pos;
			*tcheck=current->tcheck;
			*tmod=current->tmod;
			*filesize=current->file_size;
			*flags=current->flags;
			strcpy(filename,current->file_name);
			break;
		}
	}
}
//Update position
void nfs_update_pos(int send_fid,int pos){
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(current->fd_apl==send_fid){
			current->pos=pos;
			break;
		}
	}
}
//Update check time
void update_tcheck(char *fname,long int tcheck){
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(current->file_name,fname)==0){
			current->tcheck=tcheck;
		}
	}
}
//Update mod time due to a new writing in the file
void update_tmod(char *fname,int tmod,int new_size){
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(current->file_name,fname)==0){
			current->file_size=new_size;
			if(new_size==0){current->pos=0;}
			current->tmod=tmod;
		}
	}
}
//Find tmod and file size for a specific node
int search_file_name(char *fname,int *tmod,int *size,int flags){
	
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(current->file_name,fname)==0 && current->flags==flags){
			*tmod=current->tmod;
			*size=current->file_size;
			return(current->fd_client);
		}
	}
	return(0);
}
//Update fd_client file descriptor for a specific node
void update_fd_client(char *filename,int new_client_fd,int flags){
	
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(current->file_name,filename)==0 && current->flags==flags){
			current->fd_client=new_client_fd;
		}
	}
}
//Delete a node from the list
int delete_nfs_node(int fd){
	struct nfs_node *current;
	
	for(current=head->next;;current=current->next){
		if(current->fd_apl==fd){
			current->prev->next=current->next;
			current->next->prev=current->prev;
			free(current);
			return(0);
		}
		if(current->next==head){return(-1);}
	}
}
//Find all the clients with the same filename inside list
int fd_apl_to_this_file(char *fname) {
	
	struct nfs_node *current;
	int howmany=0;
	
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(current->file_name,fname)==0){
			howmany++;
		}
	}
	return(howmany);
}
