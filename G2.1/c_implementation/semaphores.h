#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <pthread.h>

/*API: This is a library which implements binary semaphores. To use this libary
 * you need to have a variable which will be integer (ex int semid).
 * create semaphore : semid=mysem_create(semid,0) (the second argument is value in which the semaphore is initialized)
 * up : mysem_up(semid);
 * down: mysem_down(semid);
 * destroy: mysem_destroy(semid);
 * */
int mysem_create(int semid,int initial_value) {
    semid = semget(IPC_PRIVATE,1,S_IRWXU);
    semctl(semid,0,SETVAL,initial_value); // initial semaphore to initial_value value
    return semid;
}
void mysem_down(int semid) {
    int ret;
    struct sembuf op;

	op.sem_num=0;
	op.sem_flg=0;
	op.sem_op=-1;
	ret=semop(semid,&op,1);
    if(ret==-1){
        printf("Error in mysem_down().");
		exit(1);
    }
}
int mysem_up(int semid) {

    int ret;
    struct sembuf op;

    op.sem_num=0;
    op.sem_flg=0;
    op.sem_op=1;
    ret=semop(semid,&op,1);
    if(ret==-1){
        printf("[ERROR MESSAGE]::mysem_up() failed!");
        return ret;
    }
    ret=semctl(semid,0,GETVAL);
    if(ret>1){
        printf("[ERROR MESSAGE]::semaphore is already 1.");
        return(ret);
    }

    return(ret);
}
void mysem_destroy(int semid) {
    semctl(semid,0,IPC_RMID);
}
