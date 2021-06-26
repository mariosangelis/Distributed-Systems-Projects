import java.io.Serializable;
import java.net.InetAddress;
import javafx.util.Pair;

public class message_class implements Serializable {
    
    private int sequence_number;
    private int number;
    private InetAddress reply_ip;
    private int reply_port;
    private String client_id;
    
    public message_class(int sequence_number,int number,InetAddress reply_ip,int reply_port,String client_id){
        
        this.client_id=client_id;
        this.sequence_number=sequence_number;
        this.reply_ip=reply_ip;
        this.reply_port=reply_port;
        this.number=number;
        
    }
    
    public Pair <InetAddress,Integer> getAddress(){
        return new Pair(reply_ip,reply_port);
    }
    
    public int getNumber(){
        return this.number;
    }
    
    public String getClientId(){
        return this.client_id;
    }
    
    public int getSequenceNumber(){
        return this.sequence_number;
    }
    
    public void set_number(int number){
        this.number=number;
    }
    
    public void set_sequence_number(int seq){
        this.sequence_number=seq;
    }
    
}
