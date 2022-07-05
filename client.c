#include "header.h"

int signalArrived=0;

void my_handler(int sig)
{
    signalArrived++;
}


static char clientFifo[CLIENT_FIFO_NAME_LEN];


static void removeFifo(void){/* Invoked on exit to delete client FIFO */
    unlink(clientFifo);
}


int main(int argc,char *argv[]){
    int opt;
    char SERVER_FIFO_TEMPLATE[40];
    char pathToDataFile[40];
    int inputFd;

    int serverFd, clientFd;
    struct request req;
    struct response resp;

    while((opt = getopt(argc, argv, ":s:o:")) != -1)  
    {  
        switch(opt)  
        {  
            case 's':
                strcpy(SERVER_FIFO_TEMPLATE,optarg);
                break;
            case 'o':
                strcpy(pathToDataFile,optarg);
                break;
            case ':':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToDataFile", argv[0]);
                exit(EXIT_FAILURE);     
                break;  
            case '?':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToDataFile", argv[0]);
                exit(EXIT_FAILURE); 
                break; 
            case -1:
                break;
            default:
                abort (); 
        }
    }
    if(optind!=5){
        errno=EINVAL;
        fprintf(stderr, "Wrong format.\n" );
        fprintf(stderr, "Usage: %s -s pathToServerFifo -o pathToDataFile", argv[0]);
        exit(EXIT_FAILURE); 
    }

    int bytesRead;

    struct sigaction newact;
    newact.sa_handler = &my_handler;
    newact.sa_flags = 0;

    if((sigemptyset(&newact.sa_mask) == -1) || (sigaction(SIGINT, &newact, NULL) == -1) ){
        perror("Failled to install SIGINT signal handler");
        exit(EXIT_FAILURE);
    }

    inputFd = open (pathToDataFile, O_RDONLY);
    if(inputFd<0 && signalArrived==0){
        perror("Open file error");
        exit(EXIT_FAILURE); 
    }

    char request[100000];

    while(((bytesRead = read(inputFd, request, 100000)) == -1) && (errno == EINTR))
    if(bytesRead<0 && signalArrived==0){
        perror("Read error");
        exit(EXIT_FAILURE);
    }


    if(signalArrived>0){
        fprintf(stdout, "SIGINT occurs\n" );
        if (close(inputFd) < 0){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        removeFifo();
        exit(EXIT_SUCCESS);
    }
    
    int i=0, n=1;
    for(i=0;i<strlen(request); ++i){   
        if(request[i] == '\n')
            n++;
    }


    req.n = n;
    time_t     now;
    struct tm  ts;
    char       buf[80];

    // Get current time
    time(&now);

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);

    printf("%s Client PID#%d (%s) is submitting a %dx%d matrix\n", buf, getpid(), pathToDataFile, n, n);
    //printf("%s\n",request);

    if(signalArrived>0){
        fprintf(stdout, "SIGINT occurs\n" );
        if (close(inputFd) < 0){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 

        removeFifo();
        exit(EXIT_SUCCESS);
    }

    
    umask(0);
    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) getpid());
    if (mkfifo(clientFifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST && ((errno != EINTR))){
        perror("client mkfifo"); 
        exit(EXIT_FAILURE); 
    }
    if (atexit(removeFifo) != 0)
    {
        perror("atexit"); 
        exit(EXIT_FAILURE); 
    }

    req.pid = getpid();
    strcpy(req.reqMatrix, request);

    //snprintf(SERVER_FIFO_TEMPLATE,strlen(request)+strlen(pathToServerFifo), pathToServerFifo, (long) getpid(),request);
    
    serverFd = open(SERVER_FIFO_TEMPLATE, O_WRONLY && ((errno != EINTR)));
    if (serverFd == -1 && signalArrived==0 && ((errno != EINTR))){
        perror("open server fifo"); 
        exit(EXIT_FAILURE); 
    }

    if(signalArrived>0){
        fprintf(stdout, "SIGINT occurs\n" );
        
        if (close(inputFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        if (close(serverFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        removeFifo();
        exit(EXIT_SUCCESS);
    }

    double time_spent = 0.0;
    clock_t begin = clock();

    if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request) && ((errno != EINTR))){
        perror("write to server"); 
        exit(EXIT_FAILURE);
    }
    clientFd = open(clientFifo, O_RDONLY);
    if (clientFd == -1 && ((errno != EINTR))){
        perror("open client fifo"); 
        exit(EXIT_FAILURE);
    }

    if(signalArrived>0){
        fprintf(stdout, "SIGINT occurs\n" );
        
        if (close(inputFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        if (close(serverFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        if (close(clientFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 

        removeFifo();
        exit(EXIT_SUCCESS);
    }
    

    if (read(clientFd, &resp, sizeof(struct response)) != sizeof(struct response) && ((errno != EINTR))) {
        perror("Can't read response from server"); 
        exit(EXIT_FAILURE);
    }
    if(signalArrived>0){
        fprintf(stdout, "SIGINT occurs\n" );
        
        if (close(inputFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        if (close(serverFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        } 
        if (close(clientFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
        }
        removeFifo();
        exit(EXIT_SUCCESS);
    }

    time(&now);
    clock_t end = clock();
 
    // calculate elapsed time by finding difference (end - begin) and
    // dividing the difference by CLOCKS_PER_SEC to convert to seconds
    time_spent += (double)(end - begin) / CLOCKS_PER_SEC;

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    if(resp.invertable == 0){
        printf("%s Client PID#%d: the matrix is invertable, total time %f seconds, goodbye.\n", buf, getpid(), time_spent);
    }
    else{
        printf("%s Client PID#%d: the matrix is not invertable, total time %f seconds, goodbye.\n", buf, getpid(), time_spent);
    }
    
    if (close(inputFd) < 0 && ((errno != EINTR))){ 
        perror("Close error"); 
        exit(EXIT_FAILURE); 
    } 
    if (close(serverFd) < 0 && ((errno != EINTR))){ 
            perror("Close error"); 
            exit(EXIT_FAILURE); 
    } 
    if (close(clientFd) < 0 && ((errno != EINTR))){ 
        perror("Close error"); 
        exit(EXIT_FAILURE); 
    }
    
    exit(EXIT_SUCCESS);

}