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
*     September 10, 2023
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

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* Main logic of the worker thread */
/* IMPLEMENT HERE THE MAIN FUNCTION OF THE WORKER */
void *worker_main(void *arg) {
	struct timespec ts;
	struct timespec second = {1, 0};
	int* termination_flag = (int *)arg;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	printf("[#WORKER#] %.9f Worker Thread Alive!\n", TSPEC_TO_DOUBLE(ts));

	while (1) {
		if (*termination_flag)
			break;
		busywait_timespec(second);
		clock_gettime(CLOCK_MONOTONIC, &ts);
		printf("[#WORKER#] %.9f Still Alive!\n", TSPEC_TO_DOUBLE(ts));
		if (nanosleep(&second, NULL) == -1) //Checks if nanosleep fails
			perror("Error\n");
	}
	return NULL;
}
/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket)
{

	/* The connection with the client is alive here. Let's start
	 * the worker thread. */
	/* IMPLEMENT HERE THE LOGIC TO START THE WORKER THREAD. */
	pthread_t thread;
	int termination_flag = 0;
	if(pthread_create(&thread, NULL, worker_main, &termination_flag) != 0) {
		perror("Error\n");
	}

	/* We are ready to proceed with the rest of the request
	 * handling logic. */

	/* REUSE LOGIC FROM HW1 TO HANDLE THE PACKETS */
	uint64_t idReq;
	struct response res;
	struct timespec sentTS, reqLen, receiptTS, completionTS;
	

	while(1) {
		if(recv(conn_socket, &idReq, sizeof(idReq), 0) <= 0) 
			break;
		

		if(recv(conn_socket, &sentTS, sizeof(sentTS), 0) <= 0) 
			break;
		
		
		if(recv(conn_socket, &reqLen, sizeof(reqLen), 0) <= 0) 
			break;
		
		clock_gettime(CLOCK_MONOTONIC, &receiptTS);

		busywait_timespec(reqLen);	
		
		res.req_id = idReq;
		res.reserved = 0;
		res.ack = 0;

		if(send(conn_socket, &res, sizeof(res), 0) <= 0) 
			break;
		
		clock_gettime(CLOCK_MONOTONIC, &completionTS);

		printf("R%lu:%.9f,%.9f,%.9f,%.9f\n", 
			idReq, 
			TSPEC_TO_DOUBLE(sentTS), 
			TSPEC_TO_DOUBLE(reqLen), 
			TSPEC_TO_DOUBLE(receiptTS), 
			TSPEC_TO_DOUBLE(completionTS));
	}

	termination_flag = 1;
	pthread_join(thread, NULL);

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

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted);

	close(sockfd);
	return EXIT_SUCCESS;

}
