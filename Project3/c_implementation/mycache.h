struct cache_block *cache_head;
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define MAX_DATA_SIZE 1024
int cache_size;
struct cache_block{
	char file_name[100];
	char data[MAX_DATA_SIZE];
	int start_offset;
	int end_offset;
	int LRU;
	struct cache_block* next;
	struct cache_block* prev;
};
//Allocate memory for size*MAX_DATA_SIZE memory pages
void cache_init(int size){
	int i;
	cache_size=size/MAX_DATA_SIZE;
	cache_head=(struct cache_block *)malloc(sizeof(struct cache_block));
	cache_head->next=cache_head;
	cache_head->prev=cache_head;
	
	for(i=0;i<cache_size;i++){
		struct cache_block *new_node=(struct cache_block *)malloc(sizeof(struct cache_block));
		new_node->start_offset=-1;
		new_node->end_offset=-1;
		new_node->LRU=-1;
		bzero(new_node->file_name,100);
		bzero(new_node->data,MAX_DATA_SIZE);
		new_node->next=cache_head->next;
		cache_head->next->prev=new_node;
		cache_head->next=new_node;
		new_node->prev=cache_head;
	}
	printf("Cache init ok...\n");
	
}
void update_LRU_cache(struct cache_block *update_block){
	struct cache_block *current;
	struct cache_block *current_lru;
		
		for(current=cache_head->next;;current=current->next){
			if(current==cache_head){break;}
			if(current==update_block){
				current->LRU=-1;
				
				for(current_lru=cache_head->next;;current_lru=current_lru->next){
					if(current_lru==cache_head){break;}
					if(current_lru->LRU!=-1){
						current_lru->LRU++;
					}
				}
				current->LRU=0;
				break;
			}
		}
}
void update_cache(char *file_name,int start_offset,int end_offset,char *data,int data_length){
	struct cache_block *current;
	struct cache_block *current_lru;
	int add_flag=0,max=-1;
	//Try to find an empty cache block.If find is successfull,add the data in the empty cache block
	for(current=cache_head->next;;current=current->next){
		if(current==cache_head){break;}
		if(current->start_offset+current->end_offset==-2){
			add_flag=1;
			current->start_offset=start_offset;
			current->end_offset=end_offset;
			strcpy(current->file_name,file_name);
			memcpy(current->data,data,data_length);
			for(current_lru=cache_head->next;;current_lru=current_lru->next){
				if(current_lru==cache_head){break;}
				if(current_lru->LRU!=-1){
					current_lru->LRU++;
				}
			}
			current->LRU=0;
			break;
		}
	}
	//Cache is full,so find the last recently used cache block,replace it with the new one
	if(add_flag==0){
		for(current=cache_head->next;;current=current->next){
			if(current==cache_head){break;}
			if((current->LRU)>max){
				max=current->LRU;
			}
		}
		for(current=cache_head->next;;current=current->next){
			if(current==cache_head){break;}
			if(current->LRU==max){
				current->start_offset=start_offset;
				current->end_offset=end_offset;
				strcpy(current->file_name,file_name);
				memcpy(current->data,data,data_length);
				current->LRU=-1;
				
				for(current_lru=cache_head->next;;current_lru=current_lru->next){
					if(current_lru==cache_head){break;}
					if(current_lru->LRU!=-1){
						current_lru->LRU++;
					}
				}
				current->LRU=0;
				break;
			}
		}
	}
	
}
//Print cache data
void print_cache(){
	struct cache_block *current;
	printf("************************************CACHE START***************************************\n");
	for(current=cache_head->next;;current=current->next){
		if(current==cache_head){break;}
		printf("page_start=%d,page_end=%d,lru=%d\n",current->start_offset,current->end_offset,current->LRU);
	}
	printf("************************************CACHE END*****************************************\n");
}
//Delete from cache all blocks reffering to this filename 
void clear_cache(char *fname){
		
	struct cache_block *current;
	for(current=cache_head->next;;current=current->next){
		if(current==cache_head){break;}
		if(strcmp(current->file_name,fname)==0){
			current->start_offset=-1;
			current->end_offset=-1;
			bzero(current->file_name,100);
			bzero(current->data,MAX_DATA_SIZE);
			current->LRU=-1;
		}
	}
}
