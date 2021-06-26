

public class MySemaphore {

    private int counter;

    public MySemaphore(int init_val) {
        counter=init_val;
    }

    public synchronized void down() {
        while(counter==0) {
            try {
                wait();
            } 
            catch(InterruptedException ex) {}
        }
        counter--;
    }

    public synchronized void up() {
        counter++;
        notify();
      }
}
