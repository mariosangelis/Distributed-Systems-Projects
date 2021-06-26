
import java.io.Serializable;



public class discovery_message_class implements Serializable {
    
    private int service_id;
    
    public discovery_message_class(int svcid){
        this.service_id=svcid;
    }
    
    public int get_svcid(){
        return this.service_id;
    }
    
    public String toString(){
        
        return ""+this.get_svcid();
    }
    
}
