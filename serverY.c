#include "header.h"
#include <syslog.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/stat.h> 
#include <sys/mman.h>
#include <fcntl.h>


void                                   
becomeDaemon()
{
    pid_t pid;

    /* Forks the parent process */
    if ((pid = fork())<0){
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Terminates the parent process */
    if (pid > 0){
        exit(EXIT_SUCCESS);
    }

    /*The forked process is session leader */
    if (setsid() < 0){
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Second fork */
    if((pid = fork())<0){
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Parent termination */
    if (pid > 0){
        exit(EXIT_SUCCESS);
    }

    /* Unmasks */
    umask(0);

    /* Appropriated directory changing */
    chdir(".");

    /* Close core  */
    close(STDERR_FILENO);
    close(STDOUT_FILENO);
    close(STDIN_FILENO);
}
int fd;
int signalArrived=0;
int fd2, fd3;
int pfdZ[2];
int pool1;
pid_t z_workers[100];
int poolS1;
pid_t workers[100];

int global_serverFd = -7, global_dummyFd=-7, gloabal_clientFd=-7;

void cleanUPServerY(){
    if(global_serverFd != -7)
    {
        if(close(global_serverFd) == -7){
            perror("global_serverFd");
            exit(EXIT_FAILURE);
        }
    }
    if(global_dummyFd != -7)
    {
        if(close(global_dummyFd) == -7){
            perror("global_serverFd");
            exit(EXIT_FAILURE);
        }
    }
    if(gloabal_clientFd != -7)
    {
        if(close(gloabal_clientFd) == -7){
            perror("global_serverFd");
            exit(EXIT_FAILURE);
        }
    }
}
int av=0;
void ZChild_handler(int sig){
    if(sig == SIGUSR1)
        av = 1;
    if(sig == SIGINT){
        int i;
        for(i=0; i<poolS1; ++i){
            kill(z_workers[i], SIGINT);
        }
        unlink("daemonLock");
        exit(EXIT_SUCCESS);

    }
}

int childZPID;
void my_handler(int sig){
    if(sig == SIGINT){
        signalArrived++;
        unlink("daemonLock");
        kill(childZPID,SIGINT);
    }
}



void Z_handler(int sig){

    if(sig == SIGINT){
        int i;
        for(i=0; i<pool1; ++i){
            kill(z_workers[i], SIGINT);
        }
        unlink("daemonLock");
        exit(EXIT_SUCCESS);
    }
}


int N = 1;

void findCofactor(int matrix[N][N], int cof[N][N], int n, int xi, int xj){
    int i=0,j=0,k=0,l=0;
    for (k=0; k<n; k++){
        for (l=0; l<n; l++){
            if (k != xi && l != xj){ //kendi haric
                cof[i][j] = matrix[k][l];
                j++;
                if (j == n-1){
                    i++;
                    j=0;
                }
            }
        }
    }
}

int findDeterminant(int matrix[N][N], int n)
{
    if (n == 1) // base case
        return matrix[0][0];

    int detResult=0, sign=1, a=0; // Initialize result
    int cof[N][N];
    
    while(a<n){
        findCofactor(matrix, cof, n, 0, a);
        detResult += sign * matrix[0][a] * findDeterminant(cof, n - 1);
        sign = -sign;
        a++;
    }
 
    return detResult;
}


//SERVER Z

#define Server_Z "/server"

typedef struct
{
    int isAvailable;
    int next;
    int busy;
    int n;
    sem_t mutex;
    pid_t pid; /* PID of client */
    int requestNum;
    char reqMatrix[10000];
}structServerZ;

structServerZ *sem_serverZ;
int fd3;


void createSharedServerZ(int poolSize2){
    fd3 = shm_open(Server_Z, O_CREAT | O_RDWR, 0666);
    if (fd3 < 0)
    {
        unlink("daemonLock");
        perror("shem_open:");
        exit(EXIT_FAILURE);
    }

    if(ftruncate(fd3, sizeof(structServerZ)) == -1 && ((errno != EINTR)))
    {
        unlink("daemonLock");
        perror("ftruncate:");
        exit(EXIT_FAILURE);
    }

    sem_serverZ = (structServerZ *)mmap(NULL, sizeof(structServerZ), PROT_READ | PROT_WRITE, MAP_SHARED, fd3, 0);
    if(sem_serverZ == MAP_FAILED)
    {
        unlink("daemonLock");
        perror("mmap:");
        exit(EXIT_FAILURE);
    }
    if((sem_init(&(sem_serverZ->mutex), 1, 1) == -1 ) && ((errno != EINTR)))
    {
        unlink("daemonLock");
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    sem_serverZ->isAvailable = 1;
    sem_serverZ->next = 0;
    sem_serverZ->busy = 0;
    sem_serverZ->requestNum = 0;
}

void serverZ(int poolSize2,   char pathToLogFile[40], int pfd[pool1][2], int logFd, int t){
    createSharedServerZ(poolSize2);
    struct flock lock;
    time_t     now;
    struct tm  ts;
    char buf2[1000];
    char logFile[10000];
    struct sigaction newactZ;
    newactZ.sa_handler = &Z_handler;
    newactZ.sa_flags = 0;
    
    if((sigemptyset(&newactZ.sa_mask) == -1) || (sigaction(SIGINT, &newactZ, NULL) == -1) ){
        unlink("daemonLock");
        perror("Failled to install SIGUSR1 signal handler");
        exit(EXIT_FAILURE);
    }

    time(&now);
    ts = *localtime(&now);
    strftime(buf2, sizeof(buf2), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    sprintf(logFile, "%s Z:Server Z(%s, t=%d, r=%d) started\n", buf2, pathToLogFile, t, poolSize2) ;
    
    lock.l_type = F_WRLCK;
    /* Place a write lock on the file. */
    if(fcntl(logFd, F_SETLKW, &lock) == -1){
        unlink("daemonLock");
        perror("fcntl error");
        exit(EXIT_FAILURE); 
    }
    int bytesRead = strlen(logFile);
    int bytesWritten;
    while(bytesRead > 0){
        while( (bytesWritten = write(logFd, logFile, bytesRead)) == -1 && ((errno == EINTR)))
        if(bytesWritten < 0){
            unlink("daemonLock");
            perror("Write error");
            exit(EXIT_FAILURE);
        }

        bytesRead -= bytesWritten;
    }
    lock.l_type = F_UNLCK;
    if(fcntl (logFd, F_SETLKW, &lock) == -1){
        unlink("daemonLock");
        perror("fcntl error");
        exit(EXIT_FAILURE); 
    }

    int p;
    for(p=0; p<pool1; ++p){
        if (close(pfd[p][0]) == -1){   
            unlink("daemonLock");
            perror("pipe read close error - server Z");
            exit(EXIT_FAILURE);
        }
        if (close(pfd[p][1]) == -1){   
            unlink("daemonLock");
            perror("pipe write close error - server Z");
            exit(EXIT_FAILURE);
        }
    }

    if (close(pfdZ[1]) == -1){   
        unlink("daemonLock");
        perror("Z pipe write close error - server Z");
        exit(EXIT_FAILURE);
    }

    int worker=0,childNum=0,a;

    for(worker=0; childNum<poolSize2; ++worker){
        if(signalArrived>0){
            unlink("daemonLock");
            exit(EXIT_SUCCESS);
        }
        a = fork();
        if(a == -1){
            unlink("daemonLock");
            perror("fork error");
            exit(EXIT_FAILURE);
        }
        else if(a==0){                  //child işlemleri
            z_workers[childNum] = getpid();
            struct sigaction newactZ;
            newactZ.sa_handler = &ZChild_handler;
            newactZ.sa_flags = 0;
            
            if((sigemptyset(&newactZ.sa_mask) == -1) || (sigaction(SIGUSR1, &newactZ, NULL) == -1) || (sigaction(SIGINT, &newactZ, NULL) == -1)){
                unlink("daemonLock");
                perror("Failled to install SIGUSR1 signal handler");
                exit(EXIT_FAILURE);
            }

            
            if(signalArrived>0){
                unlink("daemonLock");
                exit(EXIT_SUCCESS);
            }
           
            for(;;){
                
                while(av == 0)
                    wait(NULL);
                
                if(signalArrived>0){
                    cleanUPServerY();
                    unlink("daemonLock");
                    exit(EXIT_SUCCESS);
                }
               
                int localPid = (sem_serverZ->pid), local_n = (sem_serverZ->n) ;
                char* data = (sem_serverZ->reqMatrix);
                N = (sem_serverZ->n);
                (sem_serverZ->busy)++;
                int localBusy = sem_serverZ->busy;
                
                char buf[1000];
                char logZ_worker[10000];

                time(&now);
                ts = *localtime(&now);
                strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
                sprintf(logZ_worker, "%s Z:Worker PID#%d is handling client PID#%d, matrix size %dx%d, pool busy %d/%d\n", buf, getpid(), 
                                        localPid, local_n,local_n, localBusy, poolSize2) ;
                

                lock.l_type = F_WRLCK;
                /* Place a write lock on the file. */
                if(fcntl(logFd, F_SETLKW, &lock) == -1){
                    unlink("daemonLock");
                    perror("fcntl error");
                    exit(EXIT_FAILURE); 
                }
                bytesRead = strlen(logZ_worker);
                while(bytesRead > 0){
                    while( (bytesWritten = write(logFd, logZ_worker, bytesRead)) == -1 && ((errno == EINTR)))
                    if(bytesWritten < 0){
                        unlink("daemonLock");
                        perror("Write error");
                        exit(EXIT_FAILURE);
                    }

                    bytesRead -= bytesWritten;
                }
                lock.l_type = F_UNLCK;
                if(fcntl (logFd, F_SETLKW, &lock) == -1){
                    unlink("daemonLock");
                    perror("fcntl error");
                    exit(EXIT_FAILURE); 
                }

                int x=0;
                
                int a = 0;
                char data2[100000];
                while(data[x] != '\0'){
                    if(data[x] != '\n'){
                        data2[a] = data[x];
                    }
                    else{
                        data2[a] = ',';
                    }
                    a++;
                    x++;
                }
                data2[a] = ',';
                if(signalArrived>0){
                    cleanUPServerY();
                    unlink("daemonLock");
                    exit(EXIT_SUCCESS);
                }
                //printf("Child:%d %d\n", getpid(), childNum);

                int k=0,l=0;
                const char delim[2] = ",";
                char *token;
                int matrix[sem_serverZ->n][sem_serverZ->n];
                /* get the first token */
                token = strtok(data2, delim);
                while( token != NULL ) {
                    if((l == (sem_serverZ->n)) && (k == (sem_serverZ->n))){
                        break;
                    }
                    matrix[k][l] = atoi(token);
                    
                    if(l == (sem_serverZ->n)-1){
                        k++;
                        l=0;
                    }
                    else
                        l++;
                    token = strtok(NULL, delim);
                }
              
                int invertable;
                char INVERT[7];
                if(findDeterminant(matrix,sem_serverZ->n) != 0){
                    invertable = 0;
                    strcpy(INVERT, "IS");
                }
                else{
                    strcpy(INVERT, "IS NOT");
                    invertable = -1;
                }
                char logZ_worker2[10000];
                lock.l_type = F_WRLCK;
                /* Place a write lock on the file. */
                if(fcntl(logFd, F_SETLKW, &lock) == -1){
                    unlink("daemonLock");
                    perror("fcntl error");
                    exit(EXIT_FAILURE); 
                }
                char buf2[1000];
                time(&now);
                // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
                ts = *localtime(&now);
                strftime(buf2, sizeof(buf2), "%a %Y-%m-%d %H:%M:%S %Z", &ts);

                sprintf(logZ_worker2, "%s Z:Worker PID#%d responding to client PID#%d: the matrix %s invertible\n", buf2, getpid(), localPid, INVERT);
                bytesRead = strlen(logZ_worker2);
                while(bytesRead > 0){
                    while( ((bytesWritten = write(logFd, logZ_worker2, bytesRead)) == -1) && ((errno == EINTR)))
                    if(bytesWritten < 0){
                        unlink("daemonLock");
                        perror("Write error");
                        exit(EXIT_FAILURE);
                    }

                    bytesRead -= bytesWritten;
                }
                lock.l_type = F_UNLCK;
                if(fcntl (logFd, F_SETLKW, &lock) == -1){
                    unlink("daemonLock");
                    perror("fcntl error");
                    exit(EXIT_FAILURE); 
                }
                sleep(t);

                int clientFd;
                char clientFifo[CLIENT_FIFO_NAME_LEN];
                struct response resp;
                /* Open client FIFO (previously created by client) */
                snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long) sem_serverZ->pid );
                while( ((gloabal_clientFd = clientFd = open(clientFifo, O_WRONLY) ) == -1 ) && ((errno == EINTR))  )
                if (clientFd == -1) { /* Open failed, give up on client */
                    unlink("daemonLock");
                    perror("open client fifo"); 
                    exit(EXIT_FAILURE); 
                }
                if(signalArrived>0){
                    cleanUPServerY();
                    unlink("daemonLock");
                    exit(EXIT_SUCCESS);
                }
                /* Send response and close FIFO */

                resp.invertable = invertable;
                int respo;
                while((respo = write(clientFd, &resp, sizeof(struct response)) ) != sizeof(struct response)  && ((errno == EINTR)))
                if (respo != sizeof(struct response)){
                    unlink("daemonLock");
                    fprintf(stderr, "Error writing to FIFO %s\n", clientFifo);
                    exit(EXIT_FAILURE);
                }
                while((close(clientFd)) == -1  && ((errno == EINTR)))
                if (close(clientFd) == -1){
                    unlink("daemonLock");
                    perror("Close error"); 
                    exit(EXIT_FAILURE); 
                }
                if(signalArrived>0){
                    cleanUPServerY();
                    unlink("daemonLock");
                    exit(EXIT_SUCCESS);
                }

                
                (sem_serverZ->requestNum)--;
                av--;
                (sem_serverZ->busy)--;
                if(sem_post(&(sem_serverZ->mutex)) == -1)
                {
                    unlink("daemonLock");
                    perror("sem post");
                    exit(EXIT_FAILURE);
                }
               

            }
        }
        else{ // parent
            z_workers[childNum] = a;
            childNum++;
        }
    }

    int numRead;

    for(;;){
        struct request req3;
                    
        numRead = read(pfdZ[0], &req3, sizeof(struct request));
        
        if(sem_wait(&(sem_serverZ->mutex)) == -1 && ((errno != EINTR)))
        {
            unlink("daemonLock");
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        if(numRead == -1 && signalArrived>0){
            cleanUPServerY();
            unlink("daemonLock");
            exit(EXIT_SUCCESS); 
        }
        else if(numRead == -1){
            unlink("daemonLock");
            perror("pipe read error");
            exit(EXIT_FAILURE); 
        }
        else if ( numRead != 0){
            sem_serverZ->pid = req3.pid;
            strcpy((sem_serverZ->reqMatrix), req3.reqMatrix);
            sem_serverZ->n = req3.n;
            sem_serverZ->pid = req3.pid;
            (sem_serverZ->requestNum)++;
            if(kill(z_workers[sem_serverZ->next], SIGUSR1) == -1 && ((errno != EINTR))){
                unlink("daemonLock");
                perror("kill error");
                exit(EXIT_FAILURE); 
            }
            if(sem_serverZ->next == poolSize2-1)
                sem_serverZ->next = 0;
            else
                (sem_serverZ->next)++;
        }
        if(signalArrived>0){
            cleanUPServerY();
            unlink("daemonLock");
            exit(EXIT_SUCCESS);
        }
    }
}


#define Avail "/availablity"

typedef struct
{
    sem_t mutexAvil;
    int available[10000]; 
    int numberAvailable;
    int poolS;
}structAvaile;

structAvaile *sem_stAvail;



void createSharedAvailable(int poolSize1){
    fd2 = shm_open(Avail, O_CREAT | O_RDWR, 0666);
    if (fd2 < 0 && ((errno != EINTR)))
    {
        unlink("daemonLock");
        perror("shem_open:");
        exit(EXIT_FAILURE);
    }

    if(ftruncate(fd2, sizeof(structAvaile)) == -1 && ((errno != EINTR)))
    {
        unlink("daemonLock");
        perror("ftruncate:");
        exit(EXIT_FAILURE);
    }

    sem_stAvail = (structAvaile *)mmap(NULL, sizeof(structAvaile), PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
    if(sem_stAvail == MAP_FAILED && ((errno != EINTR)))
    {
        unlink("daemonLock");
        perror("mmap:");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&(sem_stAvail->mutexAvil), 1, 1) == -1 && ((errno != EINTR)))
    {
        unlink("daemonLock");
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    sem_stAvail->poolS = poolSize1;
    sem_stAvail->numberAvailable = poolSize1;
    int i;
    for(i=0; i<poolSize1; ++i){
        sem_stAvail->available[i] = 1; // fill 1 for available
    }

}

int main(int argc,char *argv[]){
    int serverFd, dummyFd;
    struct request req;

    char SERVER_FIFO_TEMPLATE[40];
    char pathToLogFile[40];
    int poolSize1, poolSize2, t;
    int opt;

    fd = open("daemonLock", O_CREAT | O_EXCL );
    if(fd == -1)
    {
        fprintf(stderr, "\nYou can not start 2 instances of the server Y process\n\n");
        exit(EXIT_FAILURE);
    }


    while((opt = getopt(argc, argv, ":s:o:p:r:t:")) != -1)  
    {  
        switch(opt)  
        {  
            case 's':
                strcpy(SERVER_FIFO_TEMPLATE,optarg);
                break;
            case 'o':
                strcpy(pathToLogFile,optarg);
                break;
            case 'p':
                poolSize1 = atoi(optarg);
                break;
            case 'r':
                poolSize2 = atoi(optarg);
                break;
            case 't':
                t = atoi(optarg);
                break;
            case ':':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToLogFile –p poolSize -r poolSize2 -t 2", argv[0]);
                unlink("daemonLock");
                exit(EXIT_FAILURE);     
                break;  
            case '?':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToLogFile –p poolSize -r poolSize2 -t 2", argv[0]);
                unlink("daemonLock");
                exit(EXIT_FAILURE); 
                break; 
            case -1:
                break;
            default:
                abort (); 
        }
    }

    if(optind!=11){
        errno=EINVAL;
        fprintf(stderr, "Wrong format.\n" );
        fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToLogFile –p poolSize -r poolSize2 -t 2", argv[0]);
        unlink("daemonLock");
        exit(EXIT_FAILURE); 
    }

    if(poolSize1 < 2 || poolSize2 < 2){
        errno=EINVAL;
        unlink("daemonLock");
        fprintf(stderr, "Poolsize should be >=2\n" );
        fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToLogFile –p poolSize -r poolSize2 -t 2", argv[0]);
        exit(EXIT_FAILURE); 
    }
    poolS1 = poolSize1;

    struct sigaction newact;
    newact.sa_handler = &my_handler;
    newact.sa_flags = 0;

    if((sigemptyset(&newact.sa_mask) == -1) || (sigaction(SIGINT, &newact, NULL) == -1)){
        unlink("daemonLock");
        perror("Failled to install SIGINT signal handler");
        exit(EXIT_FAILURE);
    }
    createSharedAvailable(poolSize1);

    umask(0);
    if (mkfifo(SERVER_FIFO_TEMPLATE, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST){
        unlink("daemonLock");
        perror("mkfifo"); 
        exit(EXIT_FAILURE); 
    }
    becomeDaemon( );

    pool1 = poolSize1;
    int pfd[poolSize1][2];

    int p=0;
    for(p=0; p<poolSize1; ++p){
        if(pipe(pfd[p]) == -1){ 
            unlink("daemonLock");
            perror("pipe open error");
            exit(EXIT_FAILURE);
        }
    }

    
    if(pipe(pfdZ) == -1){ 
        unlink("daemonLock");
        perror("pipe open error");
        exit(EXIT_FAILURE);
    }

    int logFd=open(pathToLogFile, O_WRONLY );
    if(logFd<0){
        unlink("daemonLock");
        perror("Open file error");
        exit(EXIT_FAILURE); 
    }

    struct flock lock;

    time_t     now;
    struct tm  ts;
    char buf[1000], logZ[10000];

    // Get current time
    time(&now);

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    
    sprintf(logZ, "%s Server Y (%s), p=%d, t=%d) started\n", buf, pathToLogFile, poolSize1, t ); 

    int bytesRead = strlen(logZ);
    int bytesWritten;
    while(bytesRead > 0){
        while( (bytesWritten = write(logFd, logZ, bytesRead)) == -1 && ((errno == EINTR)))
        if(bytesWritten < 0){
            unlink("daemonLock");
            perror("Write error");
            exit(EXIT_FAILURE);
        }
        bytesRead -= bytesWritten;
    }

    int b;
    //creating process Z
    switch(b = fork())
    {
        case -1:
            unlink("daemonLock");
            perror("fork error");
            exit(EXIT_FAILURE);
        case 0:
            unlink("daemonLock");
            serverZ(poolSize2, pathToLogFile, pfd, logFd, t);
            exit(EXIT_SUCCESS);
        default:
            childZPID = b;
            break;
    }


    char buf2[1000], logZ2[10000];
    // Get current time
    time(&now);

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf2, sizeof(buf2), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    
    sprintf(logZ2, "%s Instantiated server Z\n", buf2); 

    
    //printf ("locking\n");
    /* Initialize the flock structure. */
    memset (&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    /* Place a write lock on the file. */
    if(fcntl(logFd, F_SETLKW, &lock) == -1){
        unlink("daemonLock");
        perror("fcntl error");
        exit(EXIT_FAILURE); 
    }

    bytesRead = strlen(logZ2);
    while(bytesRead > 0){
        while( (bytesWritten = write(logFd, logZ2, bytesRead)) == -1 && ((errno == EINTR)))
        if(bytesWritten < 0){
            unlink("daemonLock");
            perror("Write error");
            exit(EXIT_FAILURE);
        }
        bytesRead -= bytesWritten;
    }

    lock.l_type = F_UNLCK;
    if(fcntl (logFd, F_SETLKW, &lock) == -1){
        perror("fcntl error");
        unlink("daemonLock");
        exit(EXIT_FAILURE); 
    }


    global_serverFd = serverFd = open(SERVER_FIFO_TEMPLATE, O_RDONLY);
    if (serverFd == -1 && ((errno != EINTR))){
        unlink("daemonLock");
        perror("open client fifo"); 
        exit(EXIT_FAILURE);
    }

    global_dummyFd = dummyFd = open(SERVER_FIFO_TEMPLATE, O_WRONLY);
    if (dummyFd == -1 && ((errno != EINTR))){
        unlink("daemonLock");
        perror("open client fifo"); 
        exit(EXIT_FAILURE);
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        unlink("daemonLock");
        perror("signal error"); 
        exit(EXIT_FAILURE);
    }

    
    if(signalArrived>0){
        cleanUPServerY();
        unlink("daemonLock");
        exit(EXIT_SUCCESS);
    }

    int work=0;
    
    int childNum=0;
    int a;
    for(work=0;childNum<poolSize1;++work){
            a = fork();
            if(a == -1){
                unlink("daemonLock");
                perror("fork error");
                exit(EXIT_FAILURE);
            }
            else if(a==0){ //child
                int bi;
                for(bi=0; bi<poolSize1; ++bi){
                    if(bi != childNum){
                        if (close(pfd[bi][0]) == -1){   
                            unlink("daemonLock");
                            perror("pipe read close error - child");
                            exit(EXIT_FAILURE);
                        }
                    }
                    if (close(pfd[bi][1]) == -1){ 
                        unlink("daemonLock");  
                        perror("pipe write close error - child");
                        exit(EXIT_FAILURE);
                    }
                }
                if (close(pfdZ[1]) == -1){   
                    unlink("daemonLock");
                    perror("Z pipe write close error - child");
                    exit(EXIT_FAILURE);
                }
                if (close(pfdZ[0]) == -1){  
                    unlink("daemonLock"); 
                    perror("Z pipe read close error - child");
                    exit(EXIT_FAILURE);
                }
                
                workers[childNum] = getpid();
                for(;;){
                    size_t numRead=0;                    
                    if(signalArrived>0){
                        cleanUPServerY();
                        unlink("daemonLock");
                        exit(EXIT_SUCCESS);
                    }
                    struct request req3;
                    
                    numRead = read(pfd[childNum][0], &req3, sizeof(struct request));
                    if(sem_wait(&(sem_stAvail->mutexAvil)) == -1 && ((errno != EINTR)))
                    {
                        unlink("daemonLock");
                        perror("sem post");
                        exit(EXIT_FAILURE);
                    }
                    (sem_stAvail->numberAvailable)--;
                    (sem_stAvail->available[childNum]) = 0;
                    if(sem_post(&(sem_stAvail->mutexAvil)) == -1)
                    {
                        unlink("daemonLock");
                        perror("sempost");
                        exit(EXIT_FAILURE);
                    }
                    //numRead = read(pfd2[0], &req3, sizeof(struct request));
                    if(numRead == -1 && signalArrived>0){
                        cleanUPServerY();
                        unlink("daemonLock");
                        exit(EXIT_SUCCESS); 
                    }
                    else if(numRead == -1){
                        unlink("daemonLock");
                        perror("pipe read error");
                        exit(EXIT_FAILURE); 
                    }
                    else if ( numRead != 0){
                        time_t     now;
                        struct tm  ts;
                        char buf[1000];
                        char logZ_worker[10000];
                        time(&now);

                        ts = *localtime(&now);
                        strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
                        sprintf(logZ_worker, "%s Worker PID#%d is handling client PID#%d, matrix size %dx%d, pool busy %d/%d\n", buf, getpid(),req3.pid,req3.n,req3.n,
                                                                                ((sem_stAvail->poolS) - (sem_stAvail->numberAvailable)), (sem_stAvail->poolS) ) ;
                        
                        lock.l_type = F_WRLCK;
                        /* Place a write lock on the file. */
                        if(fcntl(logFd, F_SETLKW, &lock) == -1){
                            unlink("daemonLock");
                            perror("fcntl error");
                            exit(EXIT_FAILURE); 
                        }
                        int bytesRead = strlen(logZ_worker);
                        int bytesWritten;
                        while(bytesRead > 0){
                            while( (bytesWritten = write(logFd, logZ_worker, bytesRead)) == -1 && ((errno == EINTR)))
                            if(bytesWritten < 0){
                                unlink("daemonLock");
                                perror("Write error");
                                exit(EXIT_FAILURE);
                            }

                            bytesRead -= bytesWritten;
                        }
                        lock.l_type = F_UNLCK;
                        if(fcntl (logFd, F_SETLKW, &lock) == -1){
                            unlink("daemonLock");
                            perror("fcntl error");
                            exit(EXIT_FAILURE); 
                        }

                        N = req3.n;
                        int x=0;
                        char* data = req3.reqMatrix;
                        int a = 0;
                        char data2[100000];
                        while(data[x] != '\0'){
                            if(data[x] != '\n'){
                                data2[a] = data[x];
                            }
                            else{
                                data2[a] = ',';
                            }
                            a++;
                            x++;
                        }
                        data2[a] = ',';
                        if(signalArrived>0){
                            cleanUPServerY();
                            unlink("daemonLock");
                            exit(EXIT_SUCCESS);
                        }
                       // printf("Child:%d %d\n", getpid(), childNum);

                        int k=0,l=0;
                        const char delim[2] = ",";
                        char *token;
                        int matrix[req3.n][req3.n];
                        /* get the first token */
                        token = strtok(data2, delim);
                        while( token != NULL ) {
                            if((l == (req3.n)) && (k == (req3.n))){
                                break;
                            }
                            matrix[k][l] = atoi(token);
                            
                            if(l == req3.n-1){
                                k++;
                                l=0;
                            }
                            else
                                l++;
                            token = strtok(NULL, delim);
                        }
                      
                        int invertable;
                        char INVERT[7];
                        if(findDeterminant(matrix,req3.n) != 0){
                            invertable = 0;
                            strcpy(INVERT, "IS");
                        }
                        else{
                            strcpy(INVERT, "IS NOT");
                            invertable = -1;
                        }
                        char logZ_worker2[10000];
                        lock.l_type = F_WRLCK;
                        /* Place a write lock on the file. */
                        if(fcntl(logFd, F_SETLKW, &lock) == -1){
                            unlink("daemonLock");
                            perror("fcntl error");
                            exit(EXIT_FAILURE); 
                        }
                        char buf2[1000];
                        time(&now);
                        // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
                        ts = *localtime(&now);
                        strftime(buf2, sizeof(buf2), "%a %Y-%m-%d %H:%M:%S %Z", &ts);

                        sprintf(logZ_worker2, "%s Worker PID#%d responding to client PID#%d: the matrix %s invertible\n", buf2, getpid(), req3.pid, INVERT);
                        bytesRead = strlen(logZ_worker2);
                        while(bytesRead > 0){
                            while( ((bytesWritten = write(logFd, logZ_worker2, bytesRead)) == -1) && ((errno == EINTR)))
                            if(bytesWritten < 0){
                                unlink("daemonLock");
                                perror("Write error");
                                exit(EXIT_FAILURE);
                            }

                            bytesRead -= bytesWritten;
                        }
                        lock.l_type = F_UNLCK;
                        if(fcntl (logFd, F_SETLKW, &lock) == -1){
                            unlink("daemonLock");
                            perror("fcntl error");
                            exit(EXIT_FAILURE); 
                        }
                        sleep(t);

                        int clientFd;
                        char clientFifo[CLIENT_FIFO_NAME_LEN];
                        struct response resp;

                        /* Open client FIFO (previously created by client) */
                        snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long) req3.pid);
                        while( ((clientFd = open(clientFifo, O_WRONLY) ) == -1 ) && ((errno == EINTR))  )
                        if (clientFd == -1) { /* Open failed, give up on client */
                            unlink("daemonLock");
                            perror("open client fifo"); 
                            exit(EXIT_FAILURE); 
                            //continue;
                        }
                        if(signalArrived>0){
                            cleanUPServerY();
                            unlink("daemonLock");
                            exit(EXIT_SUCCESS);
                        }
                        /* Send response and close FIFO */
                        resp.invertable = invertable;
                        int respo;
                        while((respo = write(clientFd, &resp, sizeof(struct response)) ) != sizeof(struct response)  && ((errno == EINTR)))
                        if (respo != sizeof(struct response)){
                            unlink("daemonLock");
                            fprintf(stderr, "Error writing to FIFO %s\n", clientFifo);
                            exit(EXIT_FAILURE);
                        }
                        

                        while((close(clientFd)) == -1  && ((errno == EINTR)))
                        if (close(clientFd) == -1){
                            unlink("daemonLock");
                            perror("Close error"); 
                            exit(EXIT_FAILURE); 
                        }
                        if(sem_wait(&(sem_stAvail->mutexAvil)) == -1)
                        {
                            unlink("daemonLock");
                            perror("sem post");
                            exit(EXIT_FAILURE);
                        }
                        
                        (sem_stAvail->numberAvailable)++;
                        (sem_stAvail->available[childNum]) = 1;
                        if(sem_post(&(sem_stAvail->mutexAvil)) == -1)
                        {
                            unlink("daemonLock");
                            perror("sempost");
                            exit(EXIT_FAILURE);
                        }
                        
                        
                    }
                }
            }
            else{ //parent

                workers[childNum] = a;
                childNum++;
            }
        

    }
    

    for(p=0; p<poolSize1; ++p){
        if (close(pfd[p][0]) == -1){   
            unlink("daemonLock");
            perror("pipe close read error - parent");
            exit(EXIT_FAILURE);
        }
        if(signalArrived>0){
            cleanUPServerY();
            unlink("daemonLock");
            exit(EXIT_SUCCESS);
        }
    }
    if (close(pfdZ[0]) == -1){   
        unlink("daemonLock");
        perror("Z pipe close read error - parent");
        exit(EXIT_FAILURE);
    }

    for (;;) { /* Read requests and send responses */
        if(signalArrived>0){
            cleanUPServerY();
            unlink("daemonLock");
            exit(EXIT_SUCCESS);
        }
        
        if (read(serverFd, &req, sizeof(struct request)) != sizeof(struct request)) {
            if(signalArrived>0){
                unlink("daemonLock");
                exit(EXIT_SUCCESS);
            }
        }
        
        int numWrite=0;
       
        //while( ( (numWrite = write(pfd[0][1], &req, sizeof(struct request))) < 0) & (errno == EINTR)  )
        int sem_i = 0;
      //  printf("--------------------------%d ", sem_stAvail->numberAvailable);
        if((sem_stAvail->numberAvailable) > 0){
            for(sem_i=0; sem_i<poolSize1; ++sem_i){
                if ( (sem_stAvail->available[sem_i]) == 1)
                    break;
            }
            if(write(pfd[sem_i][1], &req, sizeof(struct request)) < 0 && signalArrived>0)
            {
                cleanUPServerY();
                unlink("daemonLock");
                exit(EXIT_SUCCESS); 
            }
            if(numWrite < 0)
            {
                unlink("daemonLock");
                perror("pipe write error - parent");
                exit(EXIT_FAILURE); 
            }
        }
        else{
            char logZ_worker2[10000];
            lock.l_type = F_WRLCK;
            /* Place a write lock on the file. */
            if(fcntl(logFd, F_SETLKW, &lock) == -1){
                unlink("daemonLock");
                perror("fcntl error");
                exit(EXIT_FAILURE); 
            }
            char buf2[1000];
            time(&now);
            // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
            ts = *localtime(&now);
            strftime(buf2, sizeof(buf2), "%a %Y-%m-%d %H:%M:%S %Z", &ts);

            sprintf(logZ_worker2, "%s Forwarding request of PID#%d to Server Z: matrix size %dx%d pool busy %d/%d\n", buf2, req.pid, req.n,req.n, poolSize1,poolSize1);
            bytesRead = strlen(logZ_worker2);
            while(bytesRead > 0){
                while( ((bytesWritten = write(logFd, logZ_worker2, bytesRead)) == -1) && ((errno == EINTR)))
                if(bytesWritten < 0){
                    unlink("daemonLock");
                    perror("Write error");
                    exit(EXIT_FAILURE);
                }
                
                bytesRead -= bytesWritten;
            }
            lock.l_type = F_UNLCK;
            if(fcntl (logFd, F_SETLKW, &lock) == -1){
                unlink("daemonLock");
                perror("fcntl error");
                exit(EXIT_FAILURE); 
            }
            if(write(pfdZ[1], &req, sizeof(struct request)) < 0 && signalArrived>0)
            {
                unlink("daemonLock");
                exit(EXIT_SUCCESS); 
            }
            if(numWrite < 0)
            {
                unlink("daemonLock");
                perror("pipe write error - parent");
                exit(EXIT_FAILURE); 
            }
        
            if(signalArrived>0){
                unlink("daemonLock");
                exit(EXIT_SUCCESS);
            }
        }

    }
    
}