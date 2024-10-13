# Multi-threaded Server Project
This project contains the source code for a multi-threaded server implemented in C. The server is built incrementally, with each version adding more functionality. The following files correspond to each step in the evolution of the server:

## server.c (Server_1)
**Overview**
The initial implementation of a basic single-threaded server that listens on a specified port, receives client requests, and processes them with a busy-wait. The server simulates work by waiting for a client-specified duration before responding.

**Execution**
* Compile the program:
```bash
make
```
* Run the server (build directory):
```bash
./server <port_number> & ../client -a <arrival_rate> -s <service_rate> <port_number>
```
**Request Handling**
Upon receiving a request, the server will busy-wait for the specified time before replying.
The server outputs the following information:
```bash
R<request ID>:<sent timestamp>,<request length>,<receipt timestamp>,<completion timestamp>
```

## server_mt.c (Server_2)
**Overview**
This version introduces multi-threading. The server now spawns a worker thread that handles the request processing, while the main thread remains responsible for receiving client requests.

**Execution**
* To compile, run the make command as before
* Run the server:
```bash
./server_mt <port_number> & ../client -a <arrival_rate> -s <service_rate> -n <num_of_requests> <port_number>
```
**Key Features**
A worker thread is created using pthread_create() to handle the request processing.
The worker prints a message indicating that it is alive every second.
Output
The server outputs request processing information similar to HW1 but includes a multi-threading aspect.

## server_q.c (Server_2)
**Overview**
In this version, request queue management is introduced. The server now adds incoming requests to a shared queue, and the worker thread dequeues requests for processing.

**Execution**
* To compile, run the make command as before
* Run the server:
```bash
./server_q <port_number> & ../client -a <arrival_rate> -s <service_rate> -n <num_of_requests> <port_number>
```
**Key Features**
Shared queue between the parent and child threads.
Queue protection mechanisms are introduced to avoid corruption.
The server prints the status of the queue after each request is processed:
```php
Q:[R<request ID>,R<request ID>,...]
```

## server_lim.c (Server_3)
**Overview**
This version adds logic to reject requests if the queue becomes full, thus improving the server’s robustness when handling high traffic.

**Execution**
* To compile, run the make command as before
* Run the server with queue size parameter:
```bash
./server_lim -q <size> <port_number> & ../client -a <arrival_rate> -s <service_rate> -n <num_of_requests> <port_number>
```
**Key Features**
* Rejects requests when the queue is full, sending an acknowledgment with the reject message to the client.
Output format for rejected requests:
```bash
X<request ID>:<sent timestamp>,<request length>,<reject timestamp>
```
* Queue status and request processing details are similar to previous versions.
## server_multi.c (Server_4)
**Overview**
The final version (as of now) introduces multiple worker threads. The server spawns N worker threads that fetch requests from a shared queue and process them concurrently, improving throughput and resource utilization.

**Execution**
* To compile, run the make command as before
* Run the server with number of workers:
```bash
./server_multi -q <size> -w <workers> <port_number> & ../client -a <arrival_rate> -s <service_rate> -n <num_of_requests> <port_number>
```
**Key Features**
* Spawns N worker threads, where N is provided as a command-line parameter.
* Each worker thread processes requests from a shared queue, allowing concurrent processing.
Output format includes the thread ID of the worker thread that completed each request:
```bash
T<thread ID> R<request ID>:<sent time>,<req. length>,<receipt time>,<start time>,<completion time>
```
* The server outputs the thread ID responsible for processing each request in addition to the standard request information.
