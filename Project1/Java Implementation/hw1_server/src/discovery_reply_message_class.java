
import java.io.Serializable;
import java.net.InetAddress;




public class discovery_reply_message_class implements Serializable {
    
    private int capacity;
    private int server_port;
    private InetAddress server_ip;
    
    public discovery_reply_message_class(int capacity,int port, InetAddress ip){
        this.capacity=capacity;
        this.server_ip=ip;
        this.server_port=port;
    }
    
    public int get_port(){
        return this.server_port;
    }
    
    public InetAddress get_ip(){
        return this.server_ip;
    }
    
    public int get_capacity(){
        return this.capacity;
    }
    
}
