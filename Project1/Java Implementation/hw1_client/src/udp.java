
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import javafx.util.Pair;

public class udp {

    private DatagramSocket socket;
    private MulticastSocket multicast_socket;
    private byte[] buf;
    private InetAddress bind_address;
    private int bind_port;
    private boolean multicast;
    
    public udp(InetAddress bind_ip,int bind_port,int timeout,boolean multicast){
        
        try{
            
            this.bind_address = bind_ip;
            this.bind_port=bind_port;
            
            if(multicast==true){
                this.multicast=true;
                multicast_socket = new MulticastSocket(this.bind_port);
                multicast_socket.joinGroup(bind_address);
            }
            else{
                this.multicast=false;
                if(bind_port!=-1){
                    socket = new DatagramSocket(bind_port,bind_address);
                }
                else{
                    socket = new DatagramSocket();
                }

                if(timeout!=-1){
                    socket.setSoTimeout(timeout);
                }
            }
        }
        catch (SocketException ex) {
            System.out.println("Udp Socket Exception");
            System.exit(-1);
        } 
        catch (IOException ex) {
            System.out.println("Udp IOException");
            System.exit(-1);
        }
        
        buf=new byte[60000];
    }
    
    public void udp_Send(Object obj,Pair<InetAddress,Integer> destination_address){
        
        try{
            //Serialize the object
            ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
            ObjectOutputStream os = new ObjectOutputStream(outputStream);
            os.writeObject(obj);
            byte[] data = outputStream.toByteArray();

            DatagramPacket packet  = new DatagramPacket(data, data.length, destination_address.getKey(), destination_address.getValue());
            if(multicast==true){
                multicast_socket.send(packet);
            }
            else{
                socket.send(packet);
            }

        }
        catch(SocketException ex ){
            System.out.println("Udp send Socket Exception");
            System.exit(-1);
        } 
        catch (IOException ex) {
            System.out.println("Udp send IOException");
            System.exit(-1);
        } 
    }
    
    public Pair<Object, Pair<InetAddress, Integer>> udp_receive(){
        
        DatagramPacket incomingPacket = new DatagramPacket(buf, buf.length);
        Object rcvmsg = null;
        
        if(multicast==true){
            try {
                multicast_socket.receive(incomingPacket);
            }
            catch (SocketTimeoutException e) {
                return null;
            } 
            catch (IOException ex) {
                System.out.println("IOException");
            }
        }
        else{
            try {
                socket.receive(incomingPacket);
            }
            catch (SocketTimeoutException e) {
                //System.out.println("TIMEOUT");
                return null;
            } 
            catch (IOException ex) {
                System.out.println("IOException");
            }
        }
            
        
        try {
            //Deserialize the received object
            ByteArrayInputStream in = new ByteArrayInputStream(incomingPacket.getData());
            ObjectInputStream is = new ObjectInputStream(in);
            rcvmsg = (Object) is.readObject();
        }
        catch (ClassNotFoundException e) {
            System.out.println("Class Not Found Exception");
            System.exit(-1);
        }
        catch (IOException ex) {
            System.out.println("IOException");
        }

        InetAddress sender_address = incomingPacket.getAddress();
        int sender_port = incomingPacket.getPort();

        Pair<InetAddress,Integer> address_pair = new Pair(sender_address,sender_port);
                
        return new Pair<Object,Pair<InetAddress,Integer>> (rcvmsg,address_pair);
    }
    
}
