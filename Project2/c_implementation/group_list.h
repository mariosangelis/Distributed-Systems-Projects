struct group_node *head;
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
struct member_node{
	int member_id;
	struct member_node* next;
	struct member_node* prev;
};
struct group_node{
	struct member_node *members_list_head;
	char group_name[100];
	int members;
	int port;
	struct group_node* next;
	struct group_node* prev;
};
void group_list_init(){
	head=(struct group_node *)malloc(sizeof(struct group_node));
	head->next=head;
	head->prev=head;
}
struct group_node * group_list_add(char * name,int port){
	struct group_node *new_node=(struct group_node *)malloc(sizeof(struct group_node));
	new_node->members=0;
	new_node->port=port;
	strcpy(new_node->group_name,name);
	new_node->members_list_head=(struct member_node *)malloc(sizeof(struct member_node));
	new_node->members_list_head->next=new_node->members_list_head;
	new_node->members_list_head->prev=new_node->members_list_head;
	new_node->next=head->next;
	head->next->prev=new_node;
	head->next=new_node;
	new_node->prev=head;
	return new_node;
}
void member_list_add(int id,struct group_node *group){
	
	struct member_node *new_node=(struct member_node *)malloc(sizeof(struct member_node));
	new_node->member_id=id;
	new_node->next=group->members_list_head->next;
	group->members_list_head->next->prev=new_node;
	group->members_list_head->next=new_node;
	new_node->prev=group->members_list_head;
	
}
void group_delete(char *group_name){
	struct group_node *current;
	
	for(current=head->next;;current=current->next){
		if(strcmp(current->group_name,group_name)==0){
			current->next->prev=current->prev;
			current->prev->next=current->next;
			free(current);
			
			break;
		}
		if(current->next==head){break;}
	}
}
int member_list_delete(int id,char *group_name){
	
	struct group_node *current;
	struct member_node *curr_member;
	for(current=head->next;;current=current->next){
		if(strcmp(current->group_name,group_name)==0){
		for(curr_member=current->members_list_head->next;;curr_member=curr_member->next){
				if(curr_member->member_id==id){
					curr_member->next->prev=curr_member->prev;
					curr_member->prev->next=curr_member->next;
					free(curr_member);
					current->members--;
					return(current->members);
				}
				if(curr_member->next==current->members_list_head){break;}
			}
		}
		if(current->next==head){break;}
	}
	return(0);
}
void print_group_list(){
	struct group_node *current;
	struct member_node *curr_member;
	int i=0;
	for(current=head->next;;current=current->next){
		if(head->next==head){
			printf("Group list is empty\n");
			break;
		}
		printf(ANSI_COLOR_RED"Group : %s with %d members\n"ANSI_COLOR_RESET,current->group_name,current->members);
		for(curr_member=current->members_list_head->next;;curr_member=curr_member->next,i++){
			printf(ANSI_COLOR_RED"    Member[%d] with id:%d\n"ANSI_COLOR_RESET,i,curr_member->member_id);
			if(curr_member->next==current->members_list_head){break;}
		}
		i=0;
		if(current->next==head){break;}
	}
}
struct group_node * group_list_search(char *name){
	struct group_node *current;
	
	for(current=head->next;;current=current->next){
		if(strcmp(name,current->group_name)==0){return(current);}
		if(current->next==head){return (NULL);}
	}
	return(NULL);
}
struct member_node * group_list_id_search(int id,struct group_node * group){
	struct member_node *current;
	
	for(current=group->members_list_head->next;;current=current->next){
		if(current->member_id==id){return(current);}
		if(current->next==group->members_list_head){return (NULL);}
	}
	return(NULL);
}
