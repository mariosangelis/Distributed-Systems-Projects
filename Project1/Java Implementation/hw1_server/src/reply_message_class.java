
import java.io.Serializable;

public class reply_message_class implements Serializable{
    
    private int sequence_number;
    private String reply_message;
    private String server_id;
    
    public reply_message_class(int sequence_number,String reply_message,String server_id){
        
        this.server_id=server_id;
        this.sequence_number=sequence_number;
        this.reply_message=reply_message;
        
    }
    
    public String getReplyMessage(){
        return this.reply_message;
    }
    
    public String getServerId(){
        return this.server_id;
    }
    
    public int getSequenceNumber(){
        return this.sequence_number;
    }

    
}
