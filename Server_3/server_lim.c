/*******************************************************************************
* Dual-Threaded FIFO Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. It launches a secondary thread to
*     process incoming requests and allows to specify a maximum queue size.
*
* Usage:
*     <build directory>/server -q <queue_size> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 29, 2023
*
* Last Update:
*     September 25, 2024
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. If the queue is full at the time a
*     new request is received, the request is rejected with a negative ack.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s -q <queue size> <port_number>\n"


/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

struct request_meta {
	struct request request;
	struct timespec receipt_timestamp;
};

struct Node{
  struct request_meta req;
  struct Node *next;
};

struct queue {
    /* IMPLEMENT ME */
	struct Node *head;
	struct Node *tail;
	size_t current_queue_size;
	size_t max_queue_size;
};

struct worker_params {
    /* IMPLEMENT ME */
	struct queue * q;
	int conn_socket;
	volatile int termination_flag;
};

/* Helper function to perform queue initialization */
void queue_init(struct queue * the_queue, size_t queue_size)
{
	the_queue->head = NULL;
	the_queue->tail = NULL;
	the_queue->current_queue_size = 0;
	the_queue->max_queue_size = queue_size;
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	struct timespec rejected_timestamp;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */
	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* Make sure that the queue is not full */
	if (the_queue->current_queue_size >= the_queue->max_queue_size) {
		clock_gettime(CLOCK_MONOTONIC, &rejected_timestamp);
		retval = 1;
		printf("X%lu:%.9f,%.9f,%.9f\n",
					to_add.request.req_id,
					TSPEC_TO_DOUBLE(to_add.request.req_timestamp), 
            		TSPEC_TO_DOUBLE(to_add.request.req_length),
					TSPEC_TO_DOUBLE(rejected_timestamp));
		/* DO NOT RETURN DIRECTLY HERE */
	} else {
		struct Node* newRequest = (struct Node*)malloc(sizeof(struct Node));
		newRequest->req = to_add;
		newRequest->next = NULL;
		if(the_queue->head == NULL) {
			the_queue->head = newRequest;
			the_queue->tail = newRequest;
		} else {
			the_queue->tail->next=newRequest;
			the_queue->tail=newRequest;
		}
		the_queue->current_queue_size++;
		/* QUEUE SIGNALING FOR CONSUMER --- DO NOT TOUCH */
		sem_post(queue_notify);
	}

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	if(the_queue->head != NULL) { //Checks if queue is empty (it shouldn't be)
		struct Node* getReq = the_queue->head;
		retval = getReq->req;
		if(getReq->next != NULL) { //Checks if Node is the last in the_queue
			the_queue->head = getReq->next;
		} else { //Node is last element in queue
			the_queue->head = NULL;
			the_queue->tail = NULL;
		}
		the_queue->current_queue_size--;
		free(getReq);
	}

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Implement this method to correctly dump the status of the queue
 * following the format Q:[R<request ID>,R<request ID>,...] */
void dump_queue_status(struct queue * the_queue)
{
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	printf("Q:[");
	struct Node* current = the_queue->head;
	while (current != NULL) {
        printf("R%lu", current->req.request.req_id);
        current = current->next;
        if (current != NULL) {
            printf(",");
        }
    }
	printf("]\n");
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}


/* Main logic of the worker thread */
/* IMPLEMENT HERE THE MAIN FUNCTION OF THE WORKER */
void *worker_main(void *arg) {
    struct worker_params *workerArgs = (struct worker_params *)arg;
    struct request_meta reqM;
    struct response res;
    struct timespec startTS, completionTS;
    res.reserved = 0;
    res.ack = 0;
    
    while (!workerArgs->termination_flag) {
        reqM = get_from_queue(workerArgs->q);
		res.req_id = reqM.request.req_id;
		if (workerArgs->termination_flag) 
            break;
        clock_gettime(CLOCK_MONOTONIC, &startTS);
        busywait_timespec(reqM.request.req_length);
        send(workerArgs->conn_socket, &res, sizeof(res), 0);
        clock_gettime(CLOCK_MONOTONIC, &completionTS);
        printf("R%lu:%.9f,%.9f,%.9f,%.9f,%.9f\n", 
            reqM.request.req_id, 
            TSPEC_TO_DOUBLE(reqM.request.req_timestamp), 
            TSPEC_TO_DOUBLE(reqM.request.req_length), 
            TSPEC_TO_DOUBLE(reqM.receipt_timestamp), 
            TSPEC_TO_DOUBLE(startTS),
            TSPEC_TO_DOUBLE(completionTS));
        dump_queue_status(workerArgs->q);
    }
    return NULL;
}

int start_worker(pthread_t* p, void * params) {
	int retval;

	retval = pthread_create(p, NULL, worker_main, params);

	return retval;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, int queue_size)
{
	struct request_meta * reqM;
	struct queue the_queue;
	struct worker_params args;
	size_t in_bytes;
	
	args.q = &the_queue;
    args.conn_socket = conn_socket;
	args.termination_flag = 0;
	/* The connection with the client is alive here. Let's
	 * initialize the shared queue. */

	/* IMPLEMENT HERE ANY QUEUE INITIALIZATION LOGIC */
	queue_init(&the_queue, queue_size);
	/* Queue ready to go here. Let's start the worker thread. */
	/* IMPLEMENT HERE THE LOGIC TO START THE WORKER THREAD. */
	pthread_t thread;
	int worker_id;
	worker_id = start_worker(&thread, &args);
	if (worker_id < 0) {
		ERROR_INFO();
		perror("Unable to create worker thread");
		return;
	}
	/* We are ready to proceed with the rest of the request
	 * handling logic. */

	/* REUSE LOGIC FROM HW1 TO HANDLE THE PACKETS */

	reqM = (struct request_meta *)malloc(sizeof(struct request_meta));

	do {
		in_bytes = recv(conn_socket, &reqM->request, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &reqM->receipt_timestamp);
		/* Don't just return if in_bytes is 0 or -1. Instead
		 * skip the response and break out of the loop in an
		 * orderly fashion so that we can de-allocate the req
		 * and resp varaibles, and shutdown the socket. */
		if (in_bytes > 0) {
			if(add_to_queue(*reqM, &the_queue) != 0) {
				struct response res;
				res.req_id = reqM->request.req_id;
				res.reserved = 0;
				res.ack = 1;
				send(conn_socket, &res, sizeof(struct response), 0);
				dump_queue_status(&the_queue);
			}
		}
	} while (in_bytes > 0);

	/* Ask the worker thead to terminate */
	/* ASSERT TERMINATION FLAG FOR THE WORKER THREAD */
	args.termination_flag = 1;
	/* Make sure to wake-up any thread left stuck waiting for items in the queue. DO NOT TOUCH */
	sem_post(queue_notify);
	/* Wait for orderly termination of the worker thread */	
	/* ADD HERE LOGIC TO WAIT FOR TERMINATION OF WORKER */
	pthread_join(thread, NULL);
	/* FREE UP DATA STRUCTURES AND SHUTDOWN CONNECTION WITH CLIENT */
	free(reqM);
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval, opt;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;
	size_t queueSize;

	/* Parse all the command line arguments */
	/* IMPLEMENT ME!! */
	/* PARSE THE COMMANDS LINE: */
	/* 1. Detect the -q parameter and set aside the queue size  */
	if (argc > 3) {
		while((opt = getopt(argc, argv, "q:")) != -1)  
		{  
			switch(opt)  
			{  
				case 'q':  
					queueSize = strtol(optarg, NULL, 10);
					break;  
			}  
		}
		/* 2. Detect the port number to bind the server socket to (see HW1 and HW2) */
		socket_port = strtol(argv[3], NULL, 10);
		printf("INFO: setting server port as: %d\n", socket_port);
	} else {
		ERROR_INFO();
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}
	
	/* Now onward to create the right type of socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		ERROR_INFO();
		perror("Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Before moving forward, set socket to reuse address */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	/* Convert INADDR_ANY into network byte order */
	any_address.s_addr = htonl(INADDR_ANY);

	/* Time to bind the socket to the right port  */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = any_address;

	/* Attempt to bind the socket with the given parameters */
	retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to bind socket");
		return EXIT_FAILURE;
	}

	/* Let us now proceed to set the server to listen on the selected port */
	retval = listen(sockfd, BACKLOG_COUNT);

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to listen on socket");
		return EXIT_FAILURE;
	}

	/* Ready to accept connections! */
	printf("INFO: Waiting for incoming connection...\n");
	client_len = sizeof(struct sockaddr_in);
	accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);
	if (accepted == -1) {
		ERROR_INFO();
		perror("Unable to accept connections");
		return EXIT_FAILURE;
	}
	/* Initialize queue protection variables. DO NOT TOUCH. */
	queue_mutex = (sem_t *)malloc(sizeof(sem_t));
	queue_notify = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(queue_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue mutex");
		return EXIT_FAILURE;
	}
	retval = sem_init(queue_notify, 0, 0);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue notify");
		return EXIT_FAILURE;
	}
	/* DONE - Initialize queue protection variables. DO NOT TOUCH */
	/* Ready to handle the new connection with the client. */
	handle_connection(accepted, queueSize);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;
}
