#include <stdio.h>
#include <mqueue.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

double avgA, avgB;
int bcount, minB, minA;
int minThreadID = 1;
char* style;
double* waiTimes, * finishTimes;

int* burstCounts; // for reading from files
int done = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
struct timeval start_time;

bool isDone(int N){
	return (done >= N);
}

void incrDone(){
	done++;
}

struct Burst{

	int t_index;
	int b_index;
	double burst_amount;
	long int wall_clock_time;
};

struct Node{
	struct Burst burst;
	struct Node* next;
};

struct Node* head;
struct Node* current;

struct w_args{

	int index;
	int BCount;
	double initial_sleep;
	
};

struct file_args{
	
	int index;
	char lines[255];
};

double randm(double lambda){
    double u;
    u = random() / (RAND_MAX + 1.0);
    return -log(1- u) / lambda;
}

void displayRQ(){
	struct Node* ptr = head;
	int i = 1;
	printf("\n");
	while(ptr->next != NULL){
		printf("Node %d, burst thread_id: %d, burst_id: %d, burst_amount: %f, wall-clock time: %ld\n", i, ptr->next->burst.t_index, ptr->next->burst.b_index, ptr->next->burst.burst_amount, ptr->next->burst.wall_clock_time);
		i++;
		ptr = ptr->next;
	}
	printf("\t--End of queue\n");
}

void addToQ(struct Burst ind){
	
	
	if(head->next == NULL){
		head->next = (struct Node*)malloc(sizeof(struct Node));
		current = head->next;
		current->burst = ind;
	}else{
		current->next = (struct Node*)malloc(sizeof(struct Node));
		current = current->next;
		current->burst = ind;
	}
	
}

struct Node* SJF(int N){
	
	struct Node* ptr = head; // first element
	struct Node* tmp; // to delete
	int foundThreadIDs[N];
	double minBurst = ptr->next->burst.burst_amount;
	int count = 0;
	foundThreadIDs[count] = ptr->next->burst.t_index;
	count++;
	tmp = ptr;
	ptr = ptr->next;
	
	while(count < N && ptr->next != NULL){
		bool flag = 0;
		for(int i = 0; i < count; i++){
			if(ptr->next->burst.t_index == foundThreadIDs[i])
				flag = 1;
		}
		if(flag){
			ptr = ptr->next;
			continue;
		}

		if(minBurst > ptr->next->burst.burst_amount){
			minBurst = ptr->next->burst.burst_amount;
			tmp = ptr;
		}
		foundThreadIDs[count] = ptr->next->burst.t_index;
		count++;
		ptr = ptr->next;
		
	}
	ptr = tmp;
	tmp = ptr->next;//ptr->next will be deleted
	ptr->next = tmp->next;
	if(tmp->next == NULL)
		current = ptr;// if the last element is deleted, retrieve the current pointer
	tmp->next = NULL;
	return tmp;
}

struct Node* PRIO(int N){

	struct Node* ptr = head; // first element
	struct Node* tmp; // to delete
	int foundThreadIDs[N];
	int count = 0;
	foundThreadIDs[count] = ptr->next->burst.t_index;
	//foundThreadIDs[1] = 12;
	count++;
	tmp = ptr;
	ptr = ptr->next;
	bool flag = 0;
	while(count < N && ptr->next != NULL){
		
		if(ptr->next->burst.t_index == 1){
			tmp = ptr;
			break;
		}
		flag = 0;
		for(int i = 0; i < count; i++){
			if(ptr->next->burst.t_index == foundThreadIDs[i])
				flag = 1;
		}
		if(flag){
			ptr = ptr->next;
			continue;
		}
		
		if(ptr->next->burst.t_index < tmp->next->burst.t_index){
			tmp = ptr;
		}
		foundThreadIDs[count] = ptr->next->burst.t_index;
		count++;
		ptr = ptr->next;
		
	}
	ptr = tmp;
	tmp = ptr->next;//ptr->next will be deleted
	ptr->next = tmp->next;
	if(tmp->next == NULL)
		current = ptr;// if the last element is deleted, retrieve the current pointer
	tmp->next = NULL;
	return tmp;
}

struct Node* VRUNTIME(int N, double runtimes[]){
	
	struct Node* ptr = head; // first element
	struct Node* tmp = ptr; // will be deleted
	
	int foundThreadIDs[N];
	int count = 0;
	bool flag;
	
	while(count < N && ptr->next != NULL){
		
		flag = 0;
		for(int i = 0; i < count; i++){
			if(ptr->next->burst.t_index == foundThreadIDs[i])
				flag = 1;
		}
		
		if(flag == 1){
			ptr = ptr->next;
			continue;
		}
		
		foundThreadIDs[count] = ptr->next->burst.t_index;
		count++;
	//	printf(" \n\n ALL %d, %d\n", ptr->next->burst.t_index-1, minThreadID);
		if(runtimes[ptr->next->burst.t_index-1] <= runtimes[minThreadID-1]|| runtimes[ptr->next->burst.t_index-1] == 0){
			minThreadID = ptr->next->burst.t_index;
			tmp = ptr;//ptr->next will be deleted
			if(runtimes[ptr->next->burst.t_index-1] == 0)
				break;
		}
		ptr = ptr->next;
	}
	ptr = tmp;
	tmp = ptr->next;
	if(tmp->next == NULL)
		current = ptr;	
	ptr->next = tmp->next;
	// if the last element is deleted, retrieve the current pointer	
	double res = tmp->burst.burst_amount;
	runtimes[tmp->burst.t_index-1] += res*(0.7 + (0.3*(double)(tmp->burst.t_index)));

	for(int i = 0; i < N; i++)
		printf("Runtime %d: %f\n", i+1, runtimes[i]);
	tmp->next = NULL;
	return tmp;
	
}

struct Node* FCFS(){
	struct Node* ptr = head->next;
	head->next = ptr->next;
	ptr->next = NULL;
	
	return ptr;
}

void recurs_burst(int index, double initial_sleep, int BCount){

	usleep(initial_sleep*1000);

	//burst create
	struct Burst burst;
	burst.burst_amount = 10;
	while(burst.burst_amount < minB){
		srandom(index*1264878 + BCount*789 + time(NULL) + index*125605); // for different seeds
		burst.burst_amount = randm((double)(1/avgB));
	}
	burst.t_index = index;
	burst.b_index = bcount - BCount + 1;
	double sleept = 10;
	while(sleept < minA){
			sleept = randm((double)(1/avgA));
	}
	struct timeval current_time;
	gettimeofday(&current_time, NULL); 
	long int sec = (current_time.tv_usec - start_time.tv_usec + 1000000*(current_time.tv_sec - start_time.tv_sec));
	burst.wall_clock_time = (current_time.tv_usec/1000 + current_time.tv_sec*1000);
	printf("Burst created from thread %d, burst with id: %d, %ld ms after start, wall clock time = %ld\n", index, (bcount - BCount + 1), sec/1000, burst.wall_clock_time);
	
	// CS
	pthread_mutex_lock(&lock);
//	printf("\nThread %d acquired the lock for burst %d",index, burst.b_index );
//	gettimeofday(&current_time, NULL);
	 
//	sec = (current_time.tv_usec - start_time.tv_usec + 1000000*(current_time.tv_sec - start_time.tv_sec));
//	printf(" at %d\n",  sec/1000);
	addToQ(burst);
	printf("Ready Queue:");
	displayRQ();
	if(head->next->next == NULL)
		pthread_cond_signal(&cond1);
	pthread_mutex_unlock(&lock);
	gettimeofday(&current_time, NULL);
	printf("Thread %d released lock, will sleep for %f ms from wall_clok:  %ld\n\n", index, sleept, (current_time.tv_usec/1000 + current_time.tv_sec*1000));
	//exit
	if(BCount <= 1){
		incrDone();
		printf("Thread %d done creating burst\n", index);
		return;
	}
	return recurs_burst(index, sleept, BCount-1);
}

void *create_burst(void* arg){

	recurs_burst(((struct w_args *)arg)->index, ((struct w_args *)arg)->initial_sleep, ((struct w_args *)arg)->BCount);
	pthread_exit(0);
}


void initWait(int N){ // for the experiment
	waiTimes = malloc(sizeof(double)*N);
	finishTimes = malloc(sizeof(double)*N);
	for(int i = 0; i < N; i++){
		finishTimes[i] = 0;
		waiTimes[i] = 0;
	}
}

void *create_burst_from(void* arg){ //read from file
	
	char cn[255]; 
	strcpy(cn,((struct file_args *)arg)->lines);
	int BCount = 1;
	int index = ((struct file_args*)arg)->index;
	
	char *saveptr;
	char *sleept, *burst_t;
	bool flag = 0;
	while(1){
		if(flag == 0){
			sleept = strtok_r(cn, " ", &saveptr);
			flag = 1;
		}else
			sleept = strtok_r(NULL, " ", &saveptr);
		
		if(sleept == NULL){
			printf("Thread %d done creating burst\n", index);
			incrDone();
			break;
		}
		printf("Thread %d, will sleep for: %d\n",index, atoi(sleept));
		burst_t = strtok_r(NULL, "\n", &saveptr);
		
		
		//create
		usleep((unsigned int)atoi(sleept) * 1000);//interarrival
		struct Burst burst;
		burst.burst_amount = atoi(burst_t);
		burst.t_index = ((struct file_args*)arg)->index;
		burst.b_index = BCount;
		struct timeval current_time;
		gettimeofday(&current_time, NULL); 
		long int sec = (current_time.tv_usec - start_time.tv_usec + 1000000*(current_time.tv_sec - start_time.tv_sec));
		burst.wall_clock_time = (current_time.tv_usec/1000 + current_time.tv_sec*1000);
		printf("Burst created from thread %d, burst with id: %d, %ld ms after start, wall clock time = %ld\n", index, BCount, sec/1000, burst.wall_clock_time);
			
		// CS
		pthread_mutex_lock(&lock);
	//	printf("\nThread %d acquired the lock for burst %d",index, burst.b_index );
	//	gettimeofday(&current_time, NULL); 
	//	sec = (current_time.tv_usec - start_time.tv_usec + 1000000*(current_time.tv_sec - start_time.tv_sec));
	//	printf(" at %d\n",  sec/1000);
		addToQ(burst);
		printf("\n\nReady Queue:");
		displayRQ();
		if(head->next->next == NULL) // if the W thread added the first element to the RQ
			pthread_cond_signal(&cond1);
		gettimeofday(&current_time, NULL);
		printf("Thread %d released lock, will sleep for %d ms from wall_clok:  %ld\n\n", index, atoi(sleept), (current_time.tv_usec/1000 + current_time.tv_sec*1000));
		pthread_mutex_unlock(&lock);
		
		BCount++;// holds burst count+1 at the end
	}
	burstCounts[index] = BCount-1;
	pthread_exit(0);
}



void schedule_recurs(int N, double runtimes[]){
	if(head->next != NULL)
		pthread_mutex_lock(&lock);
	else
		pthread_cond_wait(&cond1, &lock);
	struct timeval current_time;
	gettimeofday(&current_time, NULL); 
	long int sec = (current_time.tv_usec - start_time.tv_usec + 1000000*(current_time.tv_sec - start_time.tv_sec));
	//CS
	printf("\n\nSignal received at %ld wall clock: %ld\n\n", sec/1000, current_time.tv_sec*1000 + current_time.tv_usec/1000);
	double sleept;
	struct Node* ptr;
	if(strcmp(style,"FCFS") == 0)
		 ptr = FCFS();
	else if(strcmp(style,"SJF") == 0)
		 ptr = SJF(N);
	else if(strcmp(style,"PRIO") == 0)
		 ptr = PRIO(N);
	else if(strcmp(style,"VRUNTIME") == 0)
		 ptr = VRUNTIME(N, runtimes);
	sleept = ptr->burst.burst_amount;
	printf("Scheduler will sleep for: %f miliseconds with thread: %d, burst: %d\n\n", sleept, burstCounts[ptr->burst.t_index-1], ptr->burst.b_index);
	pthread_mutex_unlock(&lock);
	
	//for experiment
	gettimeofday(&current_time, NULL);
	waiTimes[ptr->burst.t_index-1] = (waiTimes[ptr->burst.t_index-1]*(ptr->burst.b_index-1));
	waiTimes[ptr->burst.t_index-1] += ((current_time.tv_usec/1000 + current_time.tv_sec*1000) - ptr->burst.wall_clock_time);
	waiTimes[ptr->burst.t_index-1] /= ptr->burst.b_index;
		// it is like adding 3. burst's wait time so ((avg*2) + waitTime)/3 = new average
		
	//exit
	usleep((unsigned int)sleept*1000);
	if(ptr->burst.b_index >= burstCounts[ptr->burst.t_index-1]){
		gettimeofday(&current_time, NULL);
		finishTimes[ptr->burst.t_index-1] = ((current_time.tv_usec/1000 + current_time.tv_sec*1000) - (start_time.tv_usec/1000 + start_time.tv_sec*1000));
	}
	free(ptr);
	
	printf("\nRemaining RQ after execution:\n");
	displayRQ();
	gettimeofday(&current_time, NULL); 
	sec = ((current_time.tv_usec - start_time.tv_usec)/1000 + 1000*(current_time.tv_sec - start_time.tv_sec));
	printf("\n\nReleased lock from scheduler at %ld\n\n", sec/1000);

	if(isDone(N) && head->next == NULL)
		return;
	else 
		return schedule_recurs(N, runtimes);
}


void *scheduler(void* args){

	int N = *((int*)args);
//	if(strcmp(style,"VRUNTIME") == 0)
	double runtimes[N]; 
	for(int i = 0; i < N; i++)
		runtimes[i] = 0;
	schedule_recurs(N, runtimes);
	pthread_exit(0);
}

void initCounts(int N){ // for reading from file
	burstCounts = malloc(sizeof(int)*N);
}

int main(int argc, char *argv[]){
	
	gettimeofday(&start_time, NULL); 
	printf("Start Time: %ld\n", start_time.tv_sec*1000 + start_time.tv_usec/1000);
	int N = atoi(argv[1]);
	initWait(N);
	pthread_t tids[N];
	struct w_args t_args[N];
	pthread_t S;
	if (pthread_mutex_init(&lock, NULL) != 0)
		printf("Error in lock");
		
	
	
	head = (struct Node*)malloc(sizeof(struct Node));
	current = head;
	
	if(argc == 8){
		bcount = atoi(argv[2]);
		minB = atoi(argv[3]);
		avgB = atoi(argv[4]);
		minA = atoi(argv[5]);
		avgA = atoi(argv[6]);
		style = argv[7];
		burstCounts = malloc(sizeof(int)*N);
		for(int i = 0; i < N; i++){
			t_args[i].BCount = bcount;
			double timeT = 10;
			srandom(i*1264878 + i*789 + time(NULL) + i*1256);
			burstCounts[i] = bcount;
			while(timeT < minA){
				
				timeT = randm((double)(1/avgA));
			}
			t_args[i].initial_sleep = timeT;
			t_args[i].index = i+1;
		}

		
		for(int i = 0; i < N; i++){
			int works;
			works = pthread_create(&tids[i],NULL, create_burst, (void *) &(t_args[i]));
			if (works != 0) {
				printf("thread create failed \n");
				exit(1);
			}		
		}
		
		
		int sche = pthread_create(&S, NULL, scheduler, &N);
		if(sche != 0){
			printf("Create Failed\n");
			exit(0);
		}
		for(int i = 0; i < N; i++){
			int ret = pthread_join(tids[i], NULL);
			if (ret != 0) {
				printf("thread join failed \n");
				exit(0);
			}
		}
		
		sche = pthread_join(S, NULL);
		if(sche != 0){
			printf("Join Failed\n");
			exit(0);
		}
		
	}else if(argc == 5){
		style = argv[2];
		if(strcmp(argv[3],"-f") != 0){
			printf("Invalid Input\n");
			exit(0);
		}
		char* files = argv[4];
		char input[strlen(files) + 2];
		int dgt= 0;
		int temp = N;
		initCounts(N);
		struct file_args f_args[N];
		while (temp != 0) {
      	 		temp /= 10;
        		++dgt;
    		}
    		char lines[255];
    		for(int i = 0; i < N; i++){
    			strcpy(lines, ""); // avoid garbage 
			snprintf(input, sizeof(int)*(dgt+2), "%s-%d.txt", files, i+1);
			char c[255];
			FILE *fp = fopen(input,"r");
			while (fgets(c, sizeof(c), fp)) {
				strcat(lines, c);
			}
			strcpy(f_args[i].lines,lines);
			//printf("line2 %s", f_args[i].lines);
			fclose(fp);
			f_args[i].index = i+1;
			int works = pthread_create(&tids[i],NULL, create_burst_from, (void *) &(f_args[i]));
			if (works != 0) {
				printf("thread create failed \n");
				exit(0);
			}		
		}
		
		int sche = pthread_create(&S, NULL, scheduler, &N);
		if(sche != 0){
			printf("Create Failed\n");
			exit(0);
		}
		
		for(int i = 0; i < N; i++){
			int ret = pthread_join(tids[i], NULL);
			if (ret != 0) {
				printf("thread join failed \n");
				exit(0);
			}
		}
		
		sche = pthread_join(S, NULL);
		if(sche != 0){
			printf("Join Failed\n");
			exit(0);
		}
	}
	double total = 0;
	for(int i = 0; i < N; i++){
		printf("Average waiting time for thread %d : %f\n", i+1 , waiTimes[i]);
		total += waiTimes[i];
	}
	printf("\n Total average waiting time: %f\n", total/N);
	double max = 0;
	for(int i = 0; i < N; i++){
		printf("\nFinish time for thread %d : %f", i+1 , finishTimes[i]);
		if(finishTimes[i] > max)
			max = finishTimes[i];
	}
	
	printf("\n Finish Time: %f\n", max);
	printf("\n");
	//free(runtimes);
	free(waiTimes);
	free(finishTimes);
	free(burstCounts);
	free(head);
}










