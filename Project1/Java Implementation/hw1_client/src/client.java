import java.io.File;
import java.io.FileNotFoundException;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.Scanner;
import java.util.concurrent.TimeUnit;
import javafx.util.Pair;


public class client {
    
    private final Thread replyReceiverThread;
    private final int reply_port;
    private InetAddress reply_ip;
    private final MySemaphore reply_thread_started;
    private final int MULTICAST_PORT;
    private InetAddress MULTICAST_IP;
    private final int MAX_RETRANSMISSIONS;
    private int sequence_number;
    private final udp sending_socket;
    private final String client_id;
    private static MySemaphore lock;
    private final Map<String,Integer> server_list;
    private final List <String> reply_list;
    
    public client(int reply_port){
        this.reply_port=reply_port;
        this.set_ip_address();
        this.MAX_RETRANSMISSIONS=5;
        
        try {this.MULTICAST_IP=InetAddress.getByName("224.0.0.1");} 
        catch (UnknownHostException ex) {
            System.out.println("Can not bind to multicast group");
            System.exit(-1);
        }
        
        this.MULTICAST_PORT=10000;
        this.sequence_number=0;
        this.client_id=reply_ip.getHostAddress()+this.reply_port;
        this.sending_socket= new udp(null,-1,1000,false);
        this.reply_thread_started = new MySemaphore(0);
        this.lock=new MySemaphore(1);
        this.server_list=  new Hashtable<>();  
        this.reply_list= new ArrayList<>();
        this.replyReceiverThread =  new Thread(new replyReceiver());
        this.replyReceiverThread.start();
        
        reply_thread_started.down();
        System.out.println("OK from reply thread, return");
    }
    
    public int sendRequest(int number,int svcid,int reply_port){
        
        discovery_message_class discovery_message=new discovery_message_class(svcid);
        Pair<InetAddress,Integer> server_address=new Pair(MULTICAST_IP,MULTICAST_PORT);
        
        //Send a discovery message to the multicast channel
        sending_socket.udp_Send(discovery_message,server_address);
        
        discovery_reply_message_class rcvmsg;
        ack_message_class received_ack;
        int min_capacity=100000,server_port=-1;
        Pair<Object,Pair<InetAddress,Integer>>  ret;
        InetAddress server_ip=null;
        
        while(true){
            //Receive a multicast reply message from a server            
            ret=sending_socket.udp_receive();
            if(ret==null){
                break;
            }
            else{
                //Find the server with the minimum capacity
                rcvmsg = (discovery_reply_message_class) ret.getKey();
                
                if(rcvmsg.get_capacity()<min_capacity){    
                    min_capacity=rcvmsg.get_capacity();
                    server_ip=rcvmsg.get_ip();
                    server_port=rcvmsg.get_port();
                }
            }
        }
        
        //None server is available,clear servers list
        if(min_capacity==100000){
            return -1;
        }
        
        //Sequence_number is used for duplicate packets check
        sequence_number++;
        message_class sndmsg = new message_class(sequence_number,number,reply_ip,reply_port,client_id);
        server_address=new Pair(server_ip,server_port);
        
        //Send the request to the server with the minimum capacity.Use the stop&wait protocol for retransmition
        //If the server is down,we use "at most once" semantics,so the middleware tries for MAX_RETRANSMMISIONS times to resend the request message
        for(int i=0;i<MAX_RETRANSMISSIONS;i++){
            
            //System.out.println("Send message request, number= "+number);
            sending_socket.udp_Send(sndmsg,server_address);
            
            ret=sending_socket.udp_receive();
            
            if(ret==null){System.out.println("Got a timeout,resending");}
            else{
                received_ack=(ack_message_class)ret.getKey();
                if(received_ack.get_seq()==sequence_number){break;}
            }
        }
        return 1;
    }
    
    public String getReply(){
        
        lock.down();
        //The reply_list may be empty
        if(reply_list.size()==0){
            lock.up();
            return "";
        }
        
        //Use the FCFS protocol to return replies to client application 
        String reply=reply_list.get(reply_list.size()-1);
        reply_list.remove(reply_list.size()-1);
        lock.up();
        
        return reply;
    }
    
    private void set_ip_address(){
        
        Enumeration network_interfaces_list = null;
        try {
            network_interfaces_list = NetworkInterface.getNetworkInterfaces();

            while(network_interfaces_list.hasMoreElements()){
                NetworkInterface current_interface = (NetworkInterface) network_interfaces_list.nextElement();
                        
                Enumeration inet_addresses_list = current_interface.getInetAddresses();
                while (inet_addresses_list.hasMoreElements()){
                    
                    InetAddress current_inet_address = (InetAddress) inet_addresses_list.nextElement();
                    if(current_inet_address.getHostAddress().contains("127.")){break;}
                    
                    this.reply_ip=current_inet_address;       
                }
            }
        }
        catch (SocketException ex) {
            System.out.println("Socket exception inside set_ip_address method");
            System.exit(-1);
        }
    }
    
    //reply_receiver thread used from client to receive replies 
    public class replyReceiver extends Thread {
        public void run() {
            
            udp udp_obj = new udp(reply_ip,reply_port,-1,false);
            //Release the semaphore to unblock register method
            reply_thread_started.up();
            reply_message_class reply;
            
            Pair<Object,Pair<InetAddress,Integer>>  ret;
            Pair<InetAddress,Integer> server_address;
            
            //Duplicate packets check
            //We use a Map<String,Integer> (each index contains the server id and the expected sequence number from this server)
            while(true){
                
                //Block until receiving a reply
                ret=udp_obj.udp_receive();
                
                reply = (reply_message_class) ret.getKey();
                server_address=ret.getValue();
                
                int found=0;
                int exp_seq_num = -1;
                
                //Check if we have received previous reply messages from this server.
                if(server_list.containsKey(reply.getServerId())){
                    found=1;
                    exp_seq_num=server_list.get(reply.getServerId());
                }
                
                //If found=0,this is the first message from this server,so the client adds the [server_id,sequence_number of this reply message] list to the servers_list
                if(found==0){
                    System.out.println("Server not found. Add him to server list"); 
                    server_list.put(reply.getServerId(),reply.getSequenceNumber());
                    exp_seq_num=reply.getSequenceNumber();
                }
                
                if(reply.getSequenceNumber()>=exp_seq_num){
                    server_list.replace(reply.getServerId(), reply.getSequenceNumber()+1);
                    lock.down();
                    reply_list.add(0,reply.getReplyMessage());
                    lock.up();
                }
                    
                //Send a reply ACK message back to the server
                ack_message_class ack = new ack_message_class(reply.getSequenceNumber());
                udp_obj.udp_Send(ack,server_address);
            }
        }
    }
    
    public static void main(String[] args) throws InterruptedException{
        
        Scanner scanner=null;
        Scanner myInput = new Scanner(System.in);
        
        //Read 1000 numbers from a file
        try {
            scanner = new Scanner(new File("C:\\Users\\mario\\Documents\\NetBeansProjectsL\\client_server_with_load_balancing\\src\\primes.txt"));
        } 
        catch (FileNotFoundException ex) {
            System.out.println("Can not read from this file");
            System.exit(-1);
        }
        
        int reply_port = -1,i = 0,ret=0,service_id=10000;
        String try_again,reply_message;
        
        System.out.println("Give reply port: ");
        reply_port=myInput.nextInt();
        
        client my_client=new client(reply_port); 
        
        while(true){
            //Client reads a number from the file and calls sendRequest
            for(i=0;i<5;i++){
                if(scanner.hasNextInt()){
                    while(true){
                        ret=my_client.sendRequest(scanner.nextInt(),service_id,reply_port);
                        
                        if(ret==-1){
                            System.out.println("No servers are available,do you want to try again?(Y/n)");
                            try_again=myInput.next();
                            if(try_again=="n"){
                                break;
                            }
                        }
                        else{break;}
                    }
                }
                else{
                    System.out.println("There are not any prime numbers available");
                    System.exit(-1);
                }
            }
                
            //After giving 5 numbers,client tries to get reply of these requests
            TimeUnit.SECONDS.sleep(1);
            
            for(i=0;i<5;i++){
                reply_message=my_client.getReply();
                
                if(reply_message.isEmpty()==false){
                    System.out.println(reply_message);
                }
            }
        }
    }
}
