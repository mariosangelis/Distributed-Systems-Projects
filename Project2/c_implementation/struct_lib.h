#define MAX_MESSAGE_LENGTH 100
//Member sends a join_struct to the manager after establishing TCP connection
struct join_struct{
	char group_name[MAX_MESSAGE_LENGTH];
    int type;
    int sequencer;
	int member_id;
};
//Message struct.
struct send_message{
	int type;
	char payload[MAX_MESSAGE_LENGTH];
	int sequence_number;
	int sender_pid;
    int sequencer_sender_id;
	int delivery_number;
	int ack_port;
	char ack_addr[100];
};
//The args struct is passed from the join function to Receiver thread function
struct args{
	int port;
	int myid;
	int gsock;
	int ack_port;
    struct in_addr addr;
	int is_sequencer;
};
//Manager sends this struct to member who called join
struct manager_reply{
	int port;
    int type;
	int is_sequencer;
    int members;
};
