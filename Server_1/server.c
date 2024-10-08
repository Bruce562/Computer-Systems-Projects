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
* Last Update:
*     September 9, 2024
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. For debugging or more details, refer
*     to the accompanying documentation and logs.
*
*******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"

/* Utility function to add two timespec structures together. The input
 * parameter a is updated with the result of the sum. */
void timespecAdd (struct timespec * a, struct timespec * b)
{
	/* Try to add up the nsec and see if we spill over into the
	 * seconds */
	time_t addl_seconds = b->tv_sec;
	a->tv_nsec += b->tv_nsec;
	if (a->tv_nsec > NANO_IN_SEC) {
		addl_seconds += a->tv_nsec / NANO_IN_SEC;
		a->tv_nsec = a->tv_nsec % NANO_IN_SEC;
	}
	a->tv_sec += addl_seconds;
}

/* Utility function to compare two timespec structures. It returns 1
 * if a is in the future compared to b; -1 if b is in the future
 * compared to a; 0 if they are identical. */
int timespecCMP(struct timespec *a, struct timespec *b)
{
	if(a->tv_sec == b->tv_sec && a->tv_nsec == b->tv_nsec) {
		return 0;
	} else if((a->tv_sec > b->tv_sec) ||
		  (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec)) {
		return 1;
	} else {
		return -1;
	}
}

static void busywait(long sec, long nsec)
{
	struct timespec begin_timestamp, current_timestamp;
	struct timespec final_timestamp = {sec, nsec};
	clock_gettime(CLOCK_MONOTONIC, &begin_timestamp);
	timespecAdd(&final_timestamp, &begin_timestamp);
	while(timespecCMP(&current_timestamp, &final_timestamp) <= 0)
		clock_gettime(CLOCK_MONOTONIC, &current_timestamp);
}



/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
static void handle_connection(int conn_socket)
{
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

		busywait(reqLen.tv_sec, reqLen.tv_nsec);	
		
		res.requestID = idReq;
		res.field = 0;
		res.ackVal = 0;

		if(send(conn_socket, &res, sizeof(res), 0) < 0) 
			break;
		
		clock_gettime(CLOCK_MONOTONIC, &completionTS);

		printf("R%lu:%.9f,%.9f,%.9f,%.9f\n", 
			idReq, 
			TSPEC_TO_DOUBLE(sentTS), 
			TSPEC_TO_DOUBLE(reqLen), 
			TSPEC_TO_DOUBLE(receiptTS), 
			TSPEC_TO_DOUBLE(completionTS));
	}
}

/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval;
	in_port_t socket_port; //Unsigned 16-bit int
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len; //Int >32-bits socket address

	/* Get port to bind our socket to */
	if (argc > 1) {
		socket_port = strtol(argv[1], NULL, 10); //Command line argument
		printf("INFO: setting server port as: %d\n", socket_port);
	} else {
		ERROR_INFO();
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	/* Now onward to create the right type of socket */
	//socket(IPv4 protocol, socket type, lets system choose protocol)
	sockfd = socket(AF_INET, SOCK_STREAM, 0); //Assigns file descriptor

	if (sockfd < 0) {
		ERROR_INFO();
		perror("Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Before moving forward, set socket to reuse address */
	optval = 1;
	//setsockopt(file descriptor for option, lvl which option is defined, option name, address of optval, 4)
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