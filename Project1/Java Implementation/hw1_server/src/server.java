import static java.lang.String.valueOf;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Scanner;
import java.util.concurrent.TimeUnit;
import javafx.util.Pair;

public class server{
    
    private int server_port;
    private InetAddress server_ip;
    private int service_id;
    private String server_id;
    private int sequence_number;
    private Thread receiverThread;
    private Thread multicastReceiverThread;
    private udp sending_socket;
    private int MAX_RETRANSMISSIONS;
    private static MySemaphore lock;
    private Map<String,Integer> client_list;
    private List <Pair<Float,message_class>> request_list;
    
    public void register(int service_id,int port){
        this.server_port=port;
        this.service_id=service_id;
        this.set_ip_address();
        this.lock=new MySemaphore(1);
        this.client_list=  new Hashtable<>();  
        this.request_list= new ArrayList<>();
        this.MAX_RETRANSMISSIONS=5;
        this.sequence_number=0;
        this.server_id=server_ip.getHostAddress()+this.server_port;
        this.sending_socket= new udp(null,-1,1000,false);
        this.receiverThread =  new Thread(new receiver());
        this.receiverThread.start();
        this.multicastReceiverThread =  new Thread(new multicast_receiver());
        this.multicastReceiverThread.start();
    }
    
    public class multicast_receiver extends Thread {
        
        private int MULTICAST_PORT;
        private InetAddress MULTICAST_IP;
        private byte[] buf;
        
        public multicast_receiver(){
            MULTICAST_PORT=10000;
             try {
                this.MULTICAST_IP=InetAddress.getByName("224.0.0.1");
            } 
            catch (UnknownHostException ex) {
                System.out.println("Unknown Host Exception");
                System.exit(-1);
            }
            buf=new byte[60000];
        }

        public void run() {
            System.out.println("multicast_receiver thread just started");
            
            //Create a multicast socket and bind to the multicast address in order to "listen" discovery messages
            udp udp_obj = new udp(MULTICAST_IP,MULTICAST_PORT,-1,true);
            
            discovery_message_class discovery_message=null;
            while(true){
                
                Pair<Object,Pair<InetAddress,Integer>>  ret;
                Pair<InetAddress,Integer> client_address;
                
                ret=udp_obj.udp_receive();
                
                discovery_message = (discovery_message_class) ret.getKey();
                client_address=ret.getValue();
                
                //Send a point to point discovery reply message only if the server can serve this request
                if(discovery_message.get_svcid()==service_id){
                    
                    lock.down();
                    discovery_reply_message_class discovery_reply = new discovery_reply_message_class(request_list.size(),server_port,server_ip);
                    lock.up();
                    udp_obj.udp_Send(discovery_reply,client_address);
                }
            }
        }
    }
    
    public class receiver extends Thread {
        
        public void run() {
            Random rand = new Random();
            System.out.println("request_receiver thread just started");
            //Creates a udp socket and bind to "listen" requests from this channel
            udp udp_obj = new udp(server_ip,server_port,-1,false);
            
            message_class request=null;
            Pair<Object,Pair<InetAddress,Integer>>  ret;
            Pair<InetAddress,Integer> client_address;
                
            //We use a Map<String,Integer> (each index contains the client id and the expected sequence number from this client)
            while(true){
                //Receive a request message from a server 
                ret=udp_obj.udp_receive();
                
                request = (message_class) ret.getKey();
                client_address=ret.getValue();
                
                int found=0;
                int exp_seq_num = -1;
                
                //Check if we have received previous messages from this client
                if(client_list.containsKey(request.getClientId())){
                    found=1;
                    exp_seq_num=client_list.get(request.getClientId());
                }
                
                //If found=0,this is the first message from this client,so the server adds the [client_id,sequence_number of this request message] list to the client_list
                if(found==0){
                    System.out.println("Client not found. Add him to receiver_list"); 
                    client_list.put(request.getClientId(),request.getSequenceNumber());
                    exp_seq_num=request.getSequenceNumber();
                }
                
                if(request.getSequenceNumber()>=exp_seq_num){
                    client_list.replace(request.getClientId(), request.getSequenceNumber()+1);
                    lock.down();
                    request_list.add(0,new Pair(rand.nextFloat(),request));
                    lock.up();
                }
                
                //Send a reply ACK message back to the client
                ack_message_class ack = new ack_message_class(request.getSequenceNumber());
                udp_obj.udp_Send(ack,client_address);
            }
        }
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
                    
                    this.server_ip=current_inet_address;       
                }
            }
            System.out.println(this.server_ip);
        }
        catch (SocketException ex) {
            System.out.println("Socket exception inside set_ip_address method");
        }      
    }
    
    //getRequest function used from server application to get a request
    public Pair<Float, Integer> getRequest(){
        
        Pair <Float,Integer> ret=null;
        Pair <Float,message_class> list_pair;
        lock.down();
        
        //The receiver_list may be empty
        if(request_list.size()==0){
            lock.up();
            return null;
        }
        
        //Give the requests to the server using the FCFS(First Come First Served) protocol
        //Do not delete the request from the list
        list_pair=request_list.get(request_list.size()-1);
        ret = new Pair(list_pair.getKey(),list_pair.getValue().getNumber());
        lock.up();
        
        return ret;
    }
    
    //After serving a request,server application calls this function to send the reply to the client middleware
    public void sendReply(Float request_id,String message){
        
        lock.down();
        Pair <InetAddress,Integer> client_address=null;
        
        //Find the request with id=unique_id and set the address to client reply ip and client reply address
        for(int i=0;i<request_list.size();i++){
            if(request_list.get(i).getKey()==request_id){
                client_address=request_list.get(i).getValue().getAddress();
                //Delete the request from the list
                request_list.remove(i);
                break;
            }
        }
        lock.up();
        
        ack_message_class received_ack;
        sequence_number++;
        
        reply_message_class reply = new reply_message_class(sequence_number,message,server_id);
        
        //Send the reply to the client. Use the stop&wait protocol for retransmission
        //If the client is down,we use "at most once" semantics,so the middleware tries for MAX_RETRANSMMISIONS times to resend the reply message
        for(int i=0;i<MAX_RETRANSMISSIONS;i++){
            
            //System.out.println("Send reply message");
            sending_socket.udp_Send(reply,client_address);
            Pair<Object,Pair<InetAddress,Integer>>  ret;
            
            ret=sending_socket.udp_receive();
            
            if(ret==null){
                System.out.println("Got a timeout,resending");
            }
            else{
                received_ack=(ack_message_class)ret.getKey();
                if(received_ack.get_seq()==sequence_number){break;}
            }
        }
    }

    //Primetest method
    public int primetest(int number){
        
        int flag=0;
        int k=0;

        for(k=2;k<(number/2);k++){
            if(number%k==0){
                flag=1;
                break;
            }                
        }
        if(number==1){flag=2;}
        
        return flag;
    }

    public static void main(String[] args) throws InterruptedException {
        
        int server_port = -1;
        Scanner myInput = new Scanner(System.in);
        
        System.out.println("Give port: ");
        server_port=myInput.nextInt();
        
        int service_id=10000;
        Pair<Float, Integer> pair = null;
        
        server my_server=new server();
        my_server.register(service_id,server_port);
        
        while(true){
            TimeUnit.SECONDS.sleep(1);
            
            //Get a request
            pair = my_server.getRequest();
            
            if(pair==null){
                System.out.println("No request found");
                continue;
            }
            
            Float request_id=pair.getKey();
            int request=pair.getValue();
            String message="";
                    
            if(request!=-1){
                int ret=my_server.primetest(request);
                System.out.println("Serve request "+request);
                
                //Serve this request
                if(ret==0){ message=valueOf(request) + " is a prime number"; }
                else if(ret==1){message=valueOf(request) + " is not a prime number"; }
                else{message=valueOf(request) + " is a prime number";}
                //Send the reply back to the client
                my_server.sendReply(request_id,message);
            }
            else{
                System.out.println("No request found");
            }
        }
    }
}
