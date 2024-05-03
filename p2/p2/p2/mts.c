#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define NANOSECOND_CONVERT 1E9
#define MICROSECOND_CONVERT 1E5

// Directions
#define EAST 0
#define WEST 1

struct timespec start, stop;

//input train struct
typedef struct Train{
    int number;
    char direction;
    int loading_time;
    int crossing_time;
    pthread_cond_t *train_convar;
} train;

//train nodes struct
typedef struct TrainNode {
    train* data;
    int priority;
    struct TrainNode* next;
} trainNode;

trainNode* newTrainNode(train* data);
int isEmpty(trainNode** head);
train* peek(trainNode** head);
int peekPriority(trainNode** head);
void enqueue(trainNode** head, train* data);
int hasHigherPriority(const trainNode* a, const trainNode* b);
void dequeue(trainNode** head);

int countLines(char *l);
char* directionString(char dir);
void start_train();
void print_time();
void* train_thread(void *arg);
int nextStation(int last);
void cleanUp(FILE* fp, train* trains);
void trainDispatch(int prev);

pthread_mutex_t start_mutex;
pthread_cond_t start_condition;

pthread_mutex_t track, station;
pthread_cond_t loaded, train_convar, crossed;


int n;  //total no of input trains
int ready_to_load = 0;
int numWest, numEast = 0;   //accum no of west and east trains

trainNode* eastStation; 
trainNode* westStation;

//create new train node
trainNode* newTrainNode(train* data) {
    trainNode* newNode  = (trainNode*)malloc(sizeof(trainNode));
    newNode->data = data;
    if (data->direction == 'E' || data->direction == 'W') {
        newNode->priority = 1;
    } else {
        newNode->priority = 0;
    }
    
    newNode->next = NULL;
    return newNode;
}

//check if queue is empty
int isEmpty(trainNode** head) {
    return (*head) == NULL;
}

//check head train data
train* peek(trainNode** head) {
    if (*head == NULL) {
        return NULL;
    }
    return (*head)->data;
}

//check priority of head train
int peekPriority(trainNode** head) {
    return (*head)->priority;
}

//insert train into a queue according to the rules
void enqueue(trainNode** head, train* data) {
    trainNode* temp = newTrainNode(data);

    if (*head == NULL || hasHigherPriority(temp, *head)) {
        temp->next = *head;
        *head = temp;
    } else {
        trainNode* current = *head;

        while (current->next != NULL && (!hasHigherPriority(temp, current->next))) {
            current = current->next;
        }

        temp->next = current->next;
        current->next = temp;
    }
}

//rules to insert train in queue
int hasHigherPriority(const trainNode* a, const trainNode* b) {
    if (a->priority > b->priority) {
        return 1;
    } else if (a->priority == b->priority) {
        if (a->data->loading_time < b->data->loading_time) {
            return 1;
        } else if (a->data->loading_time == b->data->loading_time){ 
            if (a->data->number < b->data->number){
                return 1;
            }
          
        }
    }
    return 0;
}

//remove train node
void dequeue(trainNode** head) {
    if (*head == NULL) {
        return;
    }
    trainNode* temp = *head;
    (*head) = (*head)->next;
    free(temp);
}

//count total no of input trains
int countLines(char *l) {
    FILE *fp = fopen(l, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file %s\n", l);
        return -1; 
    }

    char buffer[256]; 
    int count = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        count++;
    }

    fclose(fp);
    return count;
}

// return string for direction of train
char* directionString(char dir){
    if(dir == 'e'|| dir == 'E'){
        return "East";
    }
    else{
        return "West";
    }
}

//send signal to load trains and start timer 
void start_train(){

    pthread_mutex_lock(&start_mutex);
    ready_to_load = true;
    pthread_cond_broadcast(&start_condition);
    pthread_mutex_unlock(&start_mutex);

    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) {
        perror( "clock gettime" );
        exit( EXIT_FAILURE );
    }
}

//print current time
void print_time(){
   if( clock_gettime(CLOCK_REALTIME, &stop) == -1 ) {
      perror( "clock gettime" );
      exit( EXIT_FAILURE );
   }

    double currTime = ( stop.tv_sec - start.tv_sec ) + ( stop.tv_nsec - start.tv_nsec )/ NANOSECOND_CONVERT;
    int minutes = (int)currTime/60;
    int hours = (int)currTime/3600;
    printf("%02d:%02d:%04.1f ", hours, minutes, currTime);
}

void* train_thread(void *arg){
    train* ntrain = (train*)arg;

    pthread_mutex_lock(&start_mutex);        //wait for all trains to be ready to load
    if (!ready_to_load){
        pthread_cond_wait(&start_condition, &start_mutex);
    }
    pthread_mutex_unlock(&start_mutex);

    usleep(ntrain->loading_time * MICROSECOND_CONVERT);     //begin loading train

    print_time();
    printf("Train %2d is ready to go %4s\n", ntrain->number, directionString(ntrain->direction));

    pthread_mutex_lock(&station);
    if(ntrain->direction == 'W' || ntrain->direction == 'w') {    //enqueue trains into respective stations
        
        enqueue(&westStation, ntrain);
    } else{
        
        enqueue(&eastStation, ntrain);
    }
    pthread_mutex_unlock(&station);

    pthread_cond_signal(&loaded);          //signal when a train is loaded
    pthread_cond_wait(ntrain->train_convar, &track);      //wait for next train to cross
    print_time();
    printf("Train %2d is ON the main track going %4s\n", ntrain->number, directionString(ntrain->direction));
    
    usleep(ntrain->crossing_time * MICROSECOND_CONVERT);   //begin crossing track
    print_time();
    printf("Train %2d is OFF the main track after going %4s\n", ntrain->number, directionString(ntrain->direction));
    n--;
    pthread_mutex_unlock(&track);
    pthread_cond_signal(&crossed);          //signal when train has crossed
}

//checks crossing history and queue to decide direction of next train
int nextStation(int prev){
    if(!isEmpty(&eastStation) && !isEmpty(&westStation)){
       
        if (numEast == 3){      //starvation case
            numEast == 0;
            return WEST;
        } else if (numWest == 3){
            numWest == 0;
            return EAST;
        }

        if(peekPriority(&eastStation) < peekPriority(&westStation)){
            return WEST;
        } else if (peekPriority(&eastStation) > peekPriority(&westStation)){
            return EAST;
        } else if(prev == EAST || prev == -1){       //prev = -1 is case where no trains have crossed
            return WEST;
        } else {
            return EAST;
        }
    }
    else if (isEmpty(&eastStation) && !isEmpty(&westStation)){
        return WEST;

    }else if (!isEmpty(&eastStation) && isEmpty(&westStation)){
        return EAST;
    }
    return -2;
}

//decides next train that crosses track
void trainDispatch(int prev){
    pthread_mutex_lock(&track);
    if (isEmpty(&eastStation) && isEmpty(&westStation)) {
        pthread_cond_wait(&loaded, &track);        //wait for a train to be loaded
    }

    prev = nextStation(prev);
    if (prev == EAST){
        numEast++;
        pthread_cond_signal(peek(&eastStation)->train_convar);       //signal to train from east station
        pthread_mutex_lock(&station);
        dequeue(&eastStation);
        pthread_mutex_unlock(&station);

    } else if (prev == WEST) {
        numWest++;
        pthread_cond_signal(peek(&westStation)->train_convar);        //signal to train from west station
	    pthread_mutex_lock(&station);
        dequeue(&westStation);
        pthread_mutex_unlock(&station);
    }

    pthread_cond_wait(&crossed, &track);       //wait for current train to cross track
    pthread_mutex_unlock(&track);
    
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE* input_file = fopen(argv[1], "r");
    if (input_file == NULL) {
        perror("error in opening file");
        exit(1);
    }

    pthread_mutex_init(&start_mutex, NULL);           //initiate mutexes and convars
    pthread_cond_init(&start_condition, NULL);
    pthread_mutex_init(&station,NULL);
    pthread_cond_init(&loaded, NULL);
    pthread_mutex_init(&track,NULL);
    pthread_cond_init(&crossed, NULL);

    n = countLines(argv[1]); 

    pthread_t threads[n];
    pthread_cond_t conditions[n];
    pthread_cond_init(&train_convar, NULL);
   
    for(int i=0; i< n;i++){
        pthread_cond_init(&conditions[i], NULL);
    }

    train* train_id = malloc(n * sizeof(*train_id));

    int train_no = 0;                              //parsing through input file
    char direction[2], crossTime[5], loadTime[5];
    while (fscanf(input_file,"%s %s %s", direction, loadTime, crossTime)==3) {
        train_id[train_no].number = train_no;
        train_id[train_no].direction = direction[0];
        train_id[train_no].loading_time = atoi(loadTime);
        train_id[train_no].crossing_time = atoi(crossTime);
        train_id[train_no].train_convar = &conditions[train_no];
        train_no++;
    }

    for(int i=0; i < n; i++){
        if(pthread_create(&threads[i], NULL, train_thread, (void *)&train_id[i])) {
            perror("ERROR; pthread_create()");
        }
    }

    start_train();

    int prev = -1;
    while(n != 0){
        trainDispatch(prev);
    }

    cleanUp(input_file,train_id);

}

//clean up and terminate all open files, threads, mutexes and convars
void cleanUp(FILE* fp, train* trains){

    fclose(fp);

    pthread_exit(0);

    free(trains);
    pthread_mutex_destroy(&start_mutex);
    pthread_cond_destroy(&start_condition);
    pthread_mutex_destroy(&station);
    pthread_cond_destroy(&loaded);
    pthread_mutex_destroy(&track);
    pthread_cond_destroy(&train_convar);
    pthread_cond_destroy(&crossed);

}
