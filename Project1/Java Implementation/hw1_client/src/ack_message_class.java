
import java.io.Serializable;



public class ack_message_class implements Serializable {
    private int seq;
    
    public ack_message_class(int seq){
        this.seq=seq;
    }
    
    public int get_seq(){
        return this.seq;
    }
}
