/*******************************************************************************
* Simple FIFO Order Server Implementation
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch.
*
* Usage:
*     <build directory>/server <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 202
*
* Last Changes:
*     September 16, 2024
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. For debugging or more details, refer
*     to the accompanying documentation and logs.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

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
	"Usage: %s <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

/* Max number of requests that can be queued */
#define QUEUE_SIZE 500
int current_queue_size = 0;
volatile int termination_flag = 0;

struct Node{
  struct request req;
  struct Node *next;
};

struct queue {
    /* IMPLEMENT ME */
	struct Node *head;
	struct Node *tail;
};

struct worker_params {
    struct queue * q;
	int conn_socket;
};

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	if(current_queue_size != QUEUE_SIZE) {
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
		current_queue_size++;
	} else retval = -1;
	

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	sem_post(queue_notify);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request get_from_queue(struct queue * the_queue)
{
	struct request retval;
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
		current_queue_size--;
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
        printf("R%lu", current->req.req_id);
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
    struct request req;
    struct response res;
    struct timespec startTS, completionTS;
    res.reserved = 0;
    res.ack = 0;
    
    while (1) {
        req = get_from_queue(workerArgs->q);

		if (termination_flag) 
            break;
        clock_gettime(CLOCK_MONOTONIC, &startTS);
        busywait_timespec(req.req_length);
        
        res.req_id = req.req_id;
        if (send(workerArgs->conn_socket, &res, sizeof(res), 0) < 0) 
            break;

        clock_gettime(CLOCK_MONOTONIC, &completionTS);
        
        printf("R%lu:%.9f,%.9f,%.9f,%.9f,%.9f\n", 
            req.req_id, 
            TSPEC_TO_DOUBLE(req.req_timestamp), 
            TSPEC_TO_DOUBLE(req.req_length), 
            TSPEC_TO_DOUBLE(req.req_reciept), 
            TSPEC_TO_DOUBLE(startTS),
            TSPEC_TO_DOUBLE(completionTS));
        dump_queue_status(workerArgs->q);
    }
    return NULL;
}


/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket)
{
    struct request *req;
    struct queue the_queue;
    struct worker_params args;
    size_t in_bytes;
    args.q = &the_queue;
    args.conn_socket = conn_socket;

    /* Initialize the shared queue */
    the_queue.head = NULL;
    the_queue.tail = NULL;

    /* Start the worker thread */
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker_main, (void *)&args) != 0) {
        perror("Error: Failed to create worker thread\n");
    }

    /* Handle the packets */
    req = (struct request *)malloc(sizeof(struct request));
    do {
        in_bytes = recv(conn_socket, &req->req_id, sizeof(req->req_id), 0);
		printf("#HANDLED# in_bytes: %ld\n", in_bytes);
        if (in_bytes == 0)
			break;
		in_bytes = recv(conn_socket, &req->req_timestamp, sizeof(req->req_timestamp), 0);
		if (in_bytes == 0)
			break;
		in_bytes = recv(conn_socket, &req->req_length, sizeof(req->req_length), 0);
		if (in_bytes == 0)
			break;

        /* Sample receipt_timestamp */
        clock_gettime(CLOCK_MONOTONIC, &req->req_reciept);

        if (in_bytes > 0) {
            if (add_to_queue(*req, &the_queue) != 0)
                perror("Error: Queue is full\n");
        }
    } while (in_bytes > 0);
    /* PERFORM ORDERLY DEALLOCATION AND OUTRO HERE */
	/* Ask the worker thead to terminate */
	/* ASSERT TERMINATION FLAG FOR THE WORKER THREAD */
    termination_flag = 1;
	/* Make sure to wake-up any thread left stuck waiting for items in the queue. DO NOT TOUCH */
	sem_post(queue_notify);

	/* Wait for orderly termination of the worker thread */	
	/* ADD HERE LOGIC TO WAIT FOR TERMINATION OF WORKER */
	pthread_join(thread, NULL);
	/* FREE UP DATA STRUCTURES AND SHUTDOWN CONNECTION WITH CLIENT */
    free(req);
}



/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;

	/* Get port to bind our socket to */
	if (argc > 1) {
		socket_port = strtol(argv[1], NULL, 10);
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
	handle_connection(accepted);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;

}
