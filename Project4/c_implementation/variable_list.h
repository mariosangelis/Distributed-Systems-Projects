#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
#define INT_TYPE 10
#define MAX_NAME_SIZE 20
#define STRING_TYPE 11
#include <math.h>
struct var_node{
	char variable_name[MAX_NAME_SIZE];
	int type;
	int int_variable;
	char string_variable[MAX_NAME_SIZE];
	struct var_node *next;
	struct var_node *prev;
};
//initialize the variable list
struct var_node * variable_list_init(struct var_node *head){
	head=(struct var_node *)malloc(sizeof(struct var_node));
	head->next=head;
	head->prev=head;
	return(head);
}
//Convert a string to an integer
int my_atoi(char* snum){
	int idx, strIdx = 0, accum = 0, numIsNeg = 0;
	const unsigned int NUMLEN = (int)strlen(snum);
	
	/* Check if negative number and flag it. */
	if(snum[0] == 0x2d)
		numIsNeg = 1;
	
	for(idx = NUMLEN - 1; idx >= 0; idx--){
		/* Only process numbers from 0 through 9. */
		if(snum[strIdx] >= 0x30 && snum[strIdx] <= 0x39){
			accum += (snum[strIdx] - 0x30) * pow(10, idx);
		}
		strIdx++;
	}
	
	/* Check flag to see if originally passed -ve number and convert result if so. */
	if(!numIsNeg)
		return accum;
	else
		return accum * -1;
}
//Check if a variable exists.Return 1 if the variable exists and has integer type and 0 if the variable exists and has string type.Also,return -1 if the variable does not exist
int check_var_exists(struct var_node *head,char *name,char *string_value,int *integer_value){
	int ret=0;
	struct var_node* current;
	for(current=head->next;;current=current->next){
		if(current==head){
			return(-1);
		}
		if(strcmp(name,current->variable_name)==0){
			if(current->type==INT_TYPE){
				*integer_value=current->int_variable;
				return(1);
			}
			else{
				strcpy(string_value,current->string_variable);
				return(0);
			}
		}
	}
	return ret;
}
//Set function
struct var_node * set_function(struct var_node *head,char *string_variable,int integer_variable,char *variable_name,int *error){
	
	//Allocate memory for the new node
	struct var_node *new_node=(struct var_node *)malloc(sizeof(struct var_node));
	strcpy(new_node->variable_name,variable_name);
	if(strcmp(string_variable,"NULL")==0){
		new_node->type=INT_TYPE;
		new_node->int_variable=integer_variable;
	}
	else{
		new_node->type=STRING_TYPE;
		strcpy(new_node->string_variable,string_variable);
	}
	
	new_node->next=head->next;
	head->next->prev=new_node;
	head->next=new_node;
	new_node->prev=head;
	
	return head;
}
//Delete a list of variables
void delete_variable_list(struct var_node *head){
	
	struct var_node* current,*temp;
	for(current=head->next;;){
		if(current==head){
			free(current);
			break;
		}
		temp=current->next;
		current->prev->next=current->next;
		current->next->prev=current->prev;
		free(current);
		current=temp;
	}
}
//Add arg1 and arg2 variables and save the result to variable with name equal to variable_name
struct var_node * add_function(struct var_node *head,char *string_variable,char *variable_name,char *arg1,char *arg2,int *error){
	
	struct var_node* current,*current_arg1,*current_arg2;
	int found_variable=0,arg1_is_variable=0,arg2_is_variable=0,set_error=0;
	
	//Check if a variable with name equal to variable_name exists in the list
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(variable_name,current->variable_name)==0){
			found_variable=1;
			break;
		}
	}
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			//printf("current_arg1->var=%s\n",current_arg1->variable_name);
			if(current_arg1==head){
				printf("Unknown variable in add function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in add function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	//Save the result
	if(found_variable==1){
		if(arg1_is_variable==1 && arg2_is_variable==1){current->int_variable=current_arg1->int_variable + current_arg2->int_variable;}
		else if(arg1_is_variable==0 && arg2_is_variable==0){current->int_variable=my_atoi(arg1) + my_atoi(arg2);}
		else if(arg1_is_variable==1 && arg2_is_variable==0){current->int_variable=current_arg1->int_variable + my_atoi(arg2);}
		else if(arg1_is_variable==0 && arg2_is_variable==1){current->int_variable=my_atoi(arg1) + current_arg2->int_variable;}
	}
	else{
		
		//sprintf(str, "%d",args_counter);
		//Create a variable and save the result
		if(arg1_is_variable==1 && arg2_is_variable==1){
			
			set_function(head,"NULL",(current_arg1->int_variable + current_arg2->int_variable),variable_name,&set_error);
		}
		else if(arg1_is_variable==0 && arg2_is_variable==0){set_function(head,"NULL",(my_atoi(arg1) + my_atoi(arg2)),variable_name,&set_error);}
		else if(arg1_is_variable==1 && arg2_is_variable==0){set_function(head,"NULL",(current_arg1->int_variable + my_atoi(arg2)),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==1){set_function(head,"NULL",(my_atoi(arg1) + current_arg2->int_variable),variable_name,&set_error);}
	}
	
	return(head);
}
//Sub arg1-arg2 and save the result to variable with name equal to variable_name
struct var_node * sub_function(struct var_node *head,char *string_variable,char *variable_name,char *arg1,char *arg2,int *error){
	
	struct var_node* current,*current_arg1,*current_arg2;
	int found_variable=0,arg1_is_variable=0,arg2_is_variable=0,set_error=0;
	
	//Check if a variable with name equal to variable_name exists in the list
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(variable_name,current->variable_name)==0){
			found_variable=1;
			break;
		}
	}
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){
				printf("Unknown variable in sub function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in sub function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	//Save the result
	if(found_variable==1){
		if(arg1_is_variable==1 && arg2_is_variable==1){current->int_variable=current_arg1->int_variable - current_arg2->int_variable;}
		else if(arg1_is_variable==0 && arg2_is_variable==0){current->int_variable=my_atoi(arg1) - my_atoi(arg2);}
		else if(arg1_is_variable==1 && arg2_is_variable==0){current->int_variable=current_arg1->int_variable - my_atoi(arg2);}
		else if(arg1_is_variable==0 && arg2_is_variable==1){current->int_variable=my_atoi(arg1) - current_arg2->int_variable;}
	}
	else{
		//Create a variable and save the result
		if(arg1_is_variable==1 && arg2_is_variable==1){set_function(head,"NULL",(current_arg1->int_variable - current_arg2->int_variable),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==0){set_function(head,"NULL",(my_atoi(arg1) - my_atoi(arg2)),variable_name,&set_error);}
		else if(arg1_is_variable==1 && arg2_is_variable==0){set_function(head,"NULL",(current_arg1->int_variable - my_atoi(arg2)),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==1){set_function(head,"NULL",(my_atoi(arg1) - current_arg2->int_variable),variable_name,&set_error);}
	}
	
	return(head);
}
//Multiply arg1 and arg2 variables and save the result to variable with name equal to variable_name
struct var_node * mul_function(struct var_node *head,char *string_variable,char *variable_name,char *arg1,char *arg2,int *error){
	
	struct var_node* current,*current_arg1,*current_arg2;
	int found_variable=0,arg1_is_variable=0,arg2_is_variable=0,set_error=0;
	
	//Check if a variable with name equal to variable_name exists in the list
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(variable_name,current->variable_name)==0){
			found_variable=1;
			break;
		}
	}
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){
				printf("Unknown variable in mul function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in mul function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	//Save the result
	if(found_variable==1){
		if(arg1_is_variable==1 && arg2_is_variable==1){current->int_variable=current_arg1->int_variable * current_arg2->int_variable;}
		else if(arg1_is_variable==0 && arg2_is_variable==0){current->int_variable=my_atoi(arg1) * my_atoi(arg2);}
		else if(arg1_is_variable==1 && arg2_is_variable==0){current->int_variable=current_arg1->int_variable * my_atoi(arg2);}
		else if(arg1_is_variable==0 && arg2_is_variable==1){current->int_variable=my_atoi(arg1) * current_arg2->int_variable;}
	}
	else{
		//Create a variable and save the result
		if(arg1_is_variable==1 && arg2_is_variable==1){set_function(head,"NULL",(current_arg1->int_variable * current_arg2->int_variable),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==0){set_function(head,"NULL",(my_atoi(arg1) * my_atoi(arg2)),variable_name,&set_error);}
		else if(arg1_is_variable==1 && arg2_is_variable==0){set_function(head,"NULL",(current_arg1->int_variable * my_atoi(arg2)),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==1){set_function(head,"NULL",(my_atoi(arg1) * current_arg2->int_variable),variable_name,&set_error);}
	}
	
	return(head);
}
//Divide arg1/arg2 variables and save the result to variable with name equal to variable_name
struct var_node * div_function(struct var_node *head,char *string_variable,char *variable_name,char *arg1,char *arg2,int *error){
	
	struct var_node* current,*current_arg1,*current_arg2;
	int found_variable=0,arg1_is_variable=0,arg2_is_variable=0,set_error=0;
	
	//Check if a variable with name equal to variable_name exists in the list
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(variable_name,current->variable_name)==0){
			found_variable=1;
			break;
		}
	}
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){
				printf("Unknown variable in div function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in div function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0 && current_arg2->int_variable!=0){
				break;
			}
			else if(strcmp(arg2,current_arg2->variable_name)==0 && current_arg2->int_variable==0){
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
	}
	//Save the result
	if(found_variable==1){
		if(arg1_is_variable==1 && arg2_is_variable==1){current->int_variable=current_arg1->int_variable / current_arg2->int_variable;}
		else if(arg1_is_variable==0 && arg2_is_variable==0){
			if(my_atoi(arg2)!=0){
				current->int_variable=my_atoi(arg1) / my_atoi(arg2);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==1 && arg2_is_variable==0){
			if(my_atoi(arg2)!=0){
				current->int_variable=current_arg1->int_variable / my_atoi(arg2);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==0 && arg2_is_variable==1){current->int_variable=my_atoi(arg1) / current_arg2->int_variable;}
	}
	else{
		//Create a variable and save the result
		if(arg1_is_variable==1 && arg2_is_variable==1){set_function(head,"NULL",(current_arg1->int_variable / current_arg2->int_variable),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==0){
			if(my_atoi(arg2)!=0){
				set_function(head,"NULL",(my_atoi(arg1) / my_atoi(arg2)),variable_name,&set_error);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==1 && arg2_is_variable==0){
			if(my_atoi(arg2)!=0){
				set_function(head,"NULL",(current_arg1->int_variable / my_atoi(arg2)),variable_name,&set_error);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==0 && arg2_is_variable==1){set_function(head,"NULL",(my_atoi(arg1) / current_arg2->int_variable),variable_name,&set_error);}
	}
	return(head);
}
//Find the modulo of arg1 and arg2 variables and save the result to variable with name equal to variable_name
struct var_node * mod_function(struct var_node *head,char *string_variable,char *variable_name,char *arg1,char *arg2,int *error){
	
	struct var_node* current,*current_arg1,*current_arg2;
	int found_variable=0,arg1_is_variable=0,arg2_is_variable=0,set_error=0;
	
	//Check if a variable with name equal to variable_name exists in the list
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		if(strcmp(variable_name,current->variable_name)==0){
			found_variable=1;
			break;
		}
	}
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){
				printf("Unknown variable in mod function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in mod function\n");
				*error=1;
				return(head);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0 && current_arg2->int_variable!=0){
				break;
			}
			else if(strcmp(arg2,current_arg2->variable_name)==0 && current_arg2->int_variable==0){
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
	}
	//Save the result
	if(found_variable==1){
		if(arg1_is_variable==1 && arg2_is_variable==1){current->int_variable=current_arg1->int_variable % current_arg2->int_variable;}
		else if(arg1_is_variable==0 && arg2_is_variable==0){
			if(my_atoi(arg2)!=0){
				current->int_variable=my_atoi(arg1) % my_atoi(arg2);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==1 && arg2_is_variable==0){
			if(my_atoi(arg2)!=0){
				current->int_variable=current_arg1->int_variable % my_atoi(arg2);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==0 && arg2_is_variable==1){current->int_variable=my_atoi(arg1) % current_arg2->int_variable;}
	}
	else{
		//Create a variable and save the result
		if(arg1_is_variable==1 && arg2_is_variable==1){set_function(head,"NULL",(current_arg1->int_variable % current_arg2->int_variable),variable_name,&set_error);}
		else if(arg1_is_variable==0 && arg2_is_variable==0){
		
			if(my_atoi(arg2)!=0){
				set_function(head,"NULL",(my_atoi(arg1) % my_atoi(arg2)),variable_name,&set_error);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==1 && arg2_is_variable==0){
		
			if(my_atoi(arg2)!=0){
				set_function(head,"NULL",(current_arg1->int_variable % my_atoi(arg2)),variable_name,&set_error);
			}
			else{
				printf("Division with zero\n");
				*error=1;
				return(head);
			}
		}
		else if(arg1_is_variable==0 && arg2_is_variable==1){set_function(head,"NULL",(my_atoi(arg1) % current_arg2->int_variable),variable_name,&set_error);}
	}
	
	return(head);
}
//Sleep function
int  slp_function(struct var_node *head,char *string_variable,char *variable_name,int *error){
	
	struct var_node* current;
	int found_variable=0;
	if(my_atoi(variable_name)>0){
		//sleep(my_atoi(variable_name));
		return(my_atoi(variable_name));
	}
	else{
		for(current=head->next;;current=current->next){
			if(current==head){break;}
			if(strcmp(variable_name,current->variable_name)==0){
				found_variable=1;
				break;
			}
		}
		//Save the result
		if(found_variable==1){
			return(current->int_variable);
			//sleep(current->int_variable);
		}
		else{
			printf("Variable %s does not exist\n",variable_name);
			*error=1;
			return(-1);
		}
	}
	
	return(0);
}
//Print_function
struct var_node * print_function(struct var_node *head,char array_of_token[40][MAX_NAME_SIZE],int *error,int id,double groupid){
	
	struct var_node* current;
	
	int i;
	//Check all the string of the array_of_token[]
	printf(ANSI_COLOR_GREEN"ID->[%lf ---> %d]"ANSI_COLOR_RESET,groupid,id);
	for(i=0;i<40;i++){
		if(strlen(array_of_token[i])==0){break;}
		if(array_of_token[i][0]=='$'){
			//Find the variable in the variable list
			for(current=head->next;;current=current->next){
				if(current==head){
					printf("Unknown variable in print function\n");
					*error=1;
					return(head);
				}
				if(strcmp(array_of_token[i],current->variable_name)==0){
					
					if(current->type==INT_TYPE){
						printf(ANSI_COLOR_GREEN"%d "ANSI_COLOR_RESET,current->int_variable);
					}
					else{
						printf(ANSI_COLOR_GREEN"%s "ANSI_COLOR_RESET,current->string_variable);
					}
					break;
				}
			}
		}
		else{printf(ANSI_COLOR_GREEN"%s "ANSI_COLOR_RESET,array_of_token[i]);}
	}
	printf("\n");
	
	return(head);
}
//Check if arg1 is equal to arg2.If it is,return 0,otherwise return 1
int beq_function(struct var_node *head,char *arg1,char *arg2,int *error){
	
	struct var_node* current_arg1,*current_arg2;
	int arg1_is_variable=0,arg2_is_variable=0;
	
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){
				printf("Unknown variable in beq function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in beq function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	
	if(arg1_is_variable==1 && arg2_is_variable==1){
		if(current_arg1->int_variable==current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==0){
		if(my_atoi(arg1)== my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==1 && arg2_is_variable==0){
		if(current_arg1->int_variable== my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==1){
		if(my_atoi(arg1)==current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	
	return(0);
}
//Check if arg1 is greater than arg2.If it is,return 0,otherwise return 1
int bgt_function(struct var_node *head,char *arg1,char *arg2,int *error){
	
	struct var_node* current_arg1,*current_arg2;
	int arg1_is_variable=0,arg2_is_variable=0;
	
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){\
				printf("Unknown variable in bgt function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in bgt function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	if(arg1_is_variable==1 && arg2_is_variable==1){
		if(current_arg1->int_variable > current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==0){
		if(my_atoi(arg1) > my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==1 && arg2_is_variable==0){
		if(current_arg1->int_variable > my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==1){
		if(my_atoi(arg1) > current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	
	return(0);
}
//Check if arg1 is greater than arg2 or equal to arg2.If it is,return 0,otherwise return 1
int bge_function(struct var_node *head,char *arg1,char *arg2,int *error){
	
	struct var_node* current_arg1,*current_arg2;
	int arg1_is_variable=0,arg2_is_variable=0;
	
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){\
				printf("Unknown variable in bge function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in bge function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	if(arg1_is_variable==1 && arg2_is_variable==1){
		if(current_arg1->int_variable >= current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==0){
		if(my_atoi(arg1) >= my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==1 && arg2_is_variable==0){
		if(current_arg1->int_variable >= my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==1){
		if(my_atoi(arg1) >= current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	
	return(0);
}
//Check if arg1 is lower than arg2.If it is,return 0,otherwise return 1
int blt_function(struct var_node *head,char *arg1,char *arg2,int *error){
	
	struct var_node* current_arg1,*current_arg2;
	int arg1_is_variable=0,arg2_is_variable=0;
	
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){\
				printf("Unknown variable in blt function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in blt function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	if(arg1_is_variable==1 && arg2_is_variable==1){
		if(current_arg1->int_variable < current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==0){
		if(my_atoi(arg1) < my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==1 && arg2_is_variable==0){
		if(current_arg1->int_variable < my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==1){
		if(my_atoi(arg1) < current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	
	return(0);
}
//Check if arg1 is lower than arg2 or equal to arg2.If it is,return 0,otherwise return 1
int ble_function(struct var_node *head,char *arg1,char *arg2,int *error){
	
	struct var_node* current_arg1,*current_arg2;
	int arg1_is_variable=0,arg2_is_variable=0;
	
	//Check if arg1 is an already saved variable
	if(arg1[0]=='$'){
		arg1_is_variable=1;
		for(current_arg1=head->next;;current_arg1=current_arg1->next){
			if(current_arg1==head){\
				printf("Unknown variable in ble function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg1,current_arg1->variable_name)==0){
				break;
			}
		}
	}
	//Check if arg2 is an already saved variable
	if(arg2[0]=='$'){
		arg2_is_variable=1;
		for(current_arg2=head->next;;current_arg2=current_arg2->next){
			if(current_arg2==head){
				printf("Unknown variable in ble function\n");
				*error=1;
				return(-1);
			}
			if(strcmp(arg2,current_arg2->variable_name)==0){
				break;
			}
		}
	}
	if(arg1_is_variable==1 && arg2_is_variable==1){
		if(current_arg1->int_variable <= current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==0){
		if(my_atoi(arg1) <= my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==1 && arg2_is_variable==0){
		if(current_arg1->int_variable <= my_atoi(arg2)){return(0);}
		else{return(1);}
	}
	else if(arg1_is_variable==0 && arg2_is_variable==1){
		if(my_atoi(arg1) <= current_arg2->int_variable){return(0);}
		else{return(1);}
	}
	
	return(0);
}
void print_variable_list(struct var_node *head){
	
	struct var_node *current;
	for(current=head->next;;current=current->next){
		if(current==head){break;}
		printf("variable name=%s\n",current->variable_name);
	}
}
