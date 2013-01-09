#ifndef RTLD_NEXT
#  define _GNU_SOURCE
#endif
#include <poll.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
 

/* Define some global variables. */
int RECV_SIZE = 2048;
int ERRBADFD = 9;

/* A dictionary that keeps track of translation from proxy fd
 * to repy fd. 
 */
int MAX_SOCK_FD = 1024;
int socket_fd_dict[1024];

/* A socket that we use for file descriptors. */
int file_master_sock = -1;


/* A structure for storing network function name
 * and arguments for the network functions.
 */
typedef struct callfunc
{
  char func_name[20];
  char arg_list[2048];
} FUNCSTRUCT;


typedef struct proxy_response
{
  int err_val;
  char response[2048];
} PROXY_REPLY;




/* List of all the calls we are going to interpose on. */
int socket(int domain, int type, int protocol);
int bind(int socket, const struct sockaddr *address,
         socklen_t address_len);
int accept(int socket, struct sockaddr *address,
           socklen_t *address_len);
int connect(int socket, const struct sockaddr *address,
            socklen_t address_len);
int listen(int socket, int backlog);
int close(int sockfd);
int shutdown(int socket, int how);
int setsockopt(int socket, int level, int option_name,
               const void *option_value, socklen_t option_len);
int getsockopt(int socket, int level, int option_name,
	       void *option_value, socklen_t *option_len);
int getpeername(int socket, struct sockaddr *address,
		socklen_t *address_len);
int getsockname(int socket, struct sockaddr *address,
		socklen_t *address_len);

ssize_t send(int socket, const void *message, size_t length, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags,
             const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void *buffer, size_t length,
             int flags, struct sockaddr *address, socklen_t *address_len);


ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count); 


int ioctl(int, int, ...);
int fcntl(int, int, ...);


int select(int nfds, fd_set *readfds, fd_set *writefds, 
	   fd_set *errorfds, struct timeval *timeout);

int poll(struct pollfd *fds, nfds_t nfds, int timeout);




/* Define the actual libc networking calls for communicating with
 * the Repy proxy. */
int (*libc_socket)(int, int, int);
int (*libc_accept)(int, struct sockaddr*, socklen_t*);
int (*libc_bind)(int, const struct sockaddr*, socklen_t);
int (*libc_connect)(int, const struct sockaddr*, socklen_t);
int (*libc_setsockopt)(int, int, int, const void*, socklen_t);
int (*libc_close)(int);
int (*libc_shutdown)(int, int);
ssize_t (*libc_send)(int, const void*, size_t, int);
ssize_t (*libc_recv)(int, void*, size_t, int); 



/* Define the serializing functions. */
void serialize_sockaddr(struct sockaddr* address, char* result_buf);
void serialize_msghdr(struct msghdr* message, char* result_buf);
void serialize_iovec(struct iovec* msg_iov, char* result_buf);
void serialize_fdset(int nfds, fd_set* file_set, char* result_buf);

/* Define the deserializing function. */
void deserialize_fdset(char* fd_string, fd_set* result_fdset);
void deserialize_sockaddr(struct sockaddr* address, char *recv_buf, char *msg_buf);
void deserialize_proxy_msg(PROXY_REPLY* replystruct, char* result_buffer, int* error_value);
void deserialize_select(fd_set* readfds, fd_set* writefds, fd_set* errorfds, char* recv_buf, int* return_val);


/* The repy socket address */
char *proxy_ip = "127.0.0.1";
int proxy_port = 53678;


/* The master socket control that is used to connect to the repy server. */
int initmastersock = -1;




/* For some reason itoa is not available when we preload
 * this library, so we need to create our own.
 * Note that I added the protocol argument to make my_itoa
 * be compatible with itoa, however currently the protocol
 * is ignored and we just use base 10.
 */
void my_itoa(int value, char* buff, int protocol)
{
  sprintf(buff, "%d", value);
}




/* This is the main function that forwards the serialized api call to the repy
 * proxy and returns the result of the API call back. Any returning result is
 * placed in the result_buffer and any error is placed in error_value
 */
void forward_api_to_proxy(int sockfd, char* func_call, char* arg_list, char* result_buffer, int* error_value)
{
  /* Create the FUNCSTRUCT and malloc memory for the arg list.
   * Note that the func_name attribute has a fixed size of 20 char.
   */
  
  FUNCSTRUCT sockstruct;
  PROXY_REPLY* replystruct;

  memset(&sockstruct, 0, sizeof(sockstruct));
  memset(&replystruct, 0, sizeof(replystruct));

  /* Malloc memory for arg_list and set everything to null. */
  //sockstruct.arg_list = (char*)malloc(strlen(arg_list) + 1);
  //memset(sockstruct.arg_list, 0, sizeof(arg_list) + 1);

  //memset(sockstruct.func_name, 0, strlen(sockstruct.func_name));
  //memset(sockstruct.arg_list, 0, strlen(sockstruct.arg_list));


  /* Copy over the data to the structure. */
  strcpy(sockstruct.func_name, func_call);
  strcpy(sockstruct.arg_list, arg_list);
  printf("\nCalling function '%s'.\n", func_call);
  fflush(stdout);
  /* Send the structure over to the Repy proxy server. */
  int bytes_sent = (*libc_send)(sockfd, &sockstruct, sizeof(sockstruct), 0);

  /* Receive the response back from the Repy proxy server. */
  char recv_buf[RECV_SIZE];
  (*libc_recv)(sockfd, recv_buf, RECV_SIZE, 0);

  /* Build the reply structure from the response. */
  replystruct = (PROXY_REPLY*) recv_buf;

  deserialize_proxy_msg(replystruct, result_buffer, error_value);

}




// ######################## CREATE MASTER SOCKET ###############################

int init_master_sock() 
{

  if (initmastersock < 0) {
    /* Retrieve the libc networking calls that we need for communication. */
    *(void **)(&libc_socket) = dlsym(RTLD_NEXT, "socket");
    *(void **)(&libc_accept) = dlsym(RTLD_NEXT, "accept");
    *(void **)(&libc_bind) = dlsym(RTLD_NEXT, "bind");
    *(void **)(&libc_connect) = dlsym(RTLD_NEXT, "connect");
    *(void **)(&libc_setsockopt) = dlsym(RTLD_NEXT, "setsockopt");
    *(void **)(&libc_close) = dlsym(RTLD_NEXT, "close");
    *(void **)(&libc_shutdown) = dlsym(RTLD_NEXT, "shutdown");
    *(void **)(&libc_send) = dlsym(RTLD_NEXT, "send");
    *(void **)(&libc_recv) = dlsym(RTLD_NEXT, "recv");
    
    initmastersock = 1;
  }


  /* Declare the variables we will need. */
  int mastersockfd = -1;
  struct sockaddr_in serv_addr;

  /* Exit if we are unable to load any of it. */
  if(dlerror()) {
    errno = EACCES;
    fprintf(stderr, "Unable to configure libnetworkinterpose.so");
    exit(1);
  }


  /* Create the master socket that we will use to communicate with the Repy proxy. */
  if ((mastersockfd = (*libc_socket)(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Unable to open up a sockobj for local communication to proxy");
    exit(1);
  } 

  
  /* Create the server address. */
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(proxy_ip);
  serv_addr.sin_port = htons(proxy_port);
  
  /* Connect to the Repy server. */
  if ((*libc_connect)(mastersockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
    perror("Unable to connect to the Repy proxy.");
  }


  return mastersockfd;
}




// ######################## SOCKET CONNECTION CALLS ############################


int socket(int domain, int type, int protocol)
{
  /* Initialize everything and create the master socket */
  int sockfd = init_master_sock();

  char arg_list[30] = "";
  char buf[20] = "";

  /* Convert all the arguments to string and creat a struct for it. */
  memset(buf, 0, strlen(buf));
  my_itoa(domain, buf, 10);
  
  memset(arg_list, 0, strlen(arg_list));
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(type, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(protocol, buf, 10);
  strcat(arg_list, buf);

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "socket", arg_list, recv_buf, &err_val);
  
  if (err_val < 0) {
    int repy_sock_fd = atoi(recv_buf);
    socket_fd_dict[sockfd % MAX_SOCK_FD] = repy_sock_fd;
    return sockfd;
  }
  else
    return -1;

} 

    
int bind(int sockfd, const struct sockaddr *address, socklen_t address_len)
{
  char arg_list[50] = "";
  char buf[50] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  serialize_sockaddr(address, buf);
  strcat(arg_list, buf);
  
  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "bind", arg_list, recv_buf, &err_val);

  if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;

}


int accept(int sockfd, struct sockaddr *address, socklen_t *address_len)
{

  char arg_list[50] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
    
  char recv_buf[RECV_SIZE];
  char sockfd_buf[10];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "accept", arg_list, recv_buf, &err_val);

  if (err_val < 0) {
    /* If we were successful, then we create a new connection to the
     * repy proxy in order to handle this new socket connection. We
     * then register the new proxy socket fd with the new repy socket fd
     * that was returned.
     */
    deserialize_sockaddr(address, recv_buf, sockfd_buf);

    int new_sock_fd = init_master_sock();
    int new_repy_sock_fd = atoi(sockfd_buf);
    socket_fd_dict[new_sock_fd % MAX_SOCK_FD] = new_repy_sock_fd;

    return new_sock_fd; 
  }
  else
    return -1;

}  



int connect(int sockfd, const struct sockaddr *address, socklen_t address_len)
{
  fflush(stdout);

  char arg_list[50] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  serialize_sockaddr(address, buf);
  strcat(arg_list, buf);

  char recv_buf[RECV_SIZE];
  int err_val;
  
  printf("Arg list for connect: %s", arg_list);
  fflush(stdout);

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "connect", arg_list, recv_buf, &err_val);
  
  if (err_val < 0){
    return atoi(recv_buf);
  }
  else
    return -1;
}




int listen(int sockfd, int backlog)
{
  char arg_list[20] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(backlog, buf, 10);
  strcat(arg_list, buf);

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "listen", arg_list, recv_buf, &err_val);

  if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;

}



// ################ SEND AND RECEIVE CALLS ################################


ssize_t send(int sockfd, const void *message, size_t length, int flags)
{
  printf("Got into send.\n");
  fflush(stdout);

  char arg_list[(int)length + 20];
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  my_itoa(flags, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");


  /* We add the message as the last element in the 
   * arg list because the message might contain any character,
   * including the delimeter. */
  strncat(arg_list, (char*)message, length);


  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "send", arg_list, recv_buf, &err_val);
  printf("Returning from send.\n");
  fflush(stdout);

  if (err_val < 0)
    return (ssize_t) atoi(recv_buf); 
  else
    return -1;
  
}




ssize_t sendto(int sockfd, const void *message, size_t length, int flags,
             const struct sockaddr *dest_addr, socklen_t dest_len)
{

  printf("Got into sendto");
  fflush(stdout);	
  char arg_list[(int)length + 50];
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(flags, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");
  
  serialize_sockaddr(dest_addr, arg_list);
  strcat(arg_list, ",");

  strncat(arg_list, (char*) message, length);


  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "sendto", arg_list, recv_buf, &err_val);

  if (err_val < 0)
    return (ssize_t) atoi(recv_buf); 
  else
    return -1;
}



ssize_t recv(int sockfd, void *buffer, size_t length, int flags)
{
  printf("Got into recv.\n");
  fflush(stdout);

  char arg_list[20] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa((int) length, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(flags, buf, 10);
  strcat(arg_list, buf);
 
  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "recv", arg_list, recv_buf, &err_val);

  printf("Returning from recv.\n");
  fflush(stdout);
    /* Check to make sure there was no error. */
  if (err_val < 0) {
    strcpy(buffer, recv_buf);
    return (ssize_t) strlen(recv_buf);
  }
  else {
    errno = err_val;
    return (ssize_t) -1;
  }

}



ssize_t recvfrom(int sockfd, void *buffer, size_t length,
             int flags, struct sockaddr *address, socklen_t *address_len)
{

  printf("Got into recvfrom");
  fflush(stdout);
  char arg_list[50] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa((int) length, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(flags, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");
  

  int err_val;
  char recv_buf[length + 50];

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "recvfrom", arg_list, recv_buf, &err_val);


  /* Check to make sure there was no error. */
  if (err_val == -1) {

    char msg_buf[length + 50];

    deserialize_sockaddr(address, recv_buf, msg_buf);
    strcpy(buffer, msg_buf);

    return (ssize_t) strlen(msg_buf);
  }
  else {
    errno = err_val;
    return (ssize_t) -1;
  }

}



// ################ READ AND WRITE CALLS ###########################

ssize_t write(int sockfd, const void *message, size_t length)
{
  char arg_list[(int)length + 20];
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");


  /* We add the message as the last element in the 
   * arg list because the message might contain any character,
   * including the delimeter. */
  strncat(arg_list, (char*)message, length);


  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "write", arg_list, recv_buf, &err_val);
 
  if (err_val < 0)
    return (ssize_t) atoi(recv_buf); 
  else
    return -1;
  
}



ssize_t read(int sockfd, void *buffer, size_t length)
{
  char arg_list[20] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa((int) length, buf, 10);
  strcat(arg_list, buf);

 
  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "read", arg_list, recv_buf, &err_val);

    /* Check to make sure there was no error. */
  if (err_val < 0) {
    strcpy(buffer, recv_buf);
    return (ssize_t) strlen(recv_buf);
  }
  else {
    errno = err_val;
    return (ssize_t) -1;
  }

}




// ################ SOCKET OPTION CALLS ##########################


int fcntl(int sockfd, int cmd, ...)
{
  // BUG: If the sockfd is not a network socket (if it is a file socket)
  // Then this will segfault and crash.
  printf("Got into fcntl\n");
  fflush(stdout);

  char arg_list[50] = "";
  char buf[20] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(cmd, buf, 10);
  strcat(arg_list, buf);

  printf("Got after strcat.\n");
  fflush(stdout);

  /* Since we have a variable argument '...' we do not know
   * the length or what the arguments are. Therefore we are
   * going to use the variable argument library stdarg.h to
   * retrieve all the arguments.
   */
  va_list var_arg_list;
  int arg_val;

  /* Start processing the variable arguments. */
  va_start(var_arg_list, cmd);

  printf("Got after va_start.\n");
  fflush(stdout);

  /* Go through the arg_list and retrieve all the various
   * arguments that were provided. Assume that all the 
   * variables are of type 'int'.
   */
  while((arg_val = va_arg(var_arg_list, int)) != NULL){
    strcat(arg_list, ",");
    printf("Got here2");
    fflush(stdout);
    /* Convert and add all the arguments to the list. */
    memset(buf, 0, strlen(buf));
    my_itoa(arg_val, buf, 10);
    strcat(arg_list, buf);
  }

  printf("After while loop.\n");
  fflush(stdout);

  /* End processing the variable arguments. */
  va_end(var_arg_list);
  printf("%s", arg_list);
  printf("After va_end.\n");
  fflush(stdout);

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server.
  forward_api_to_proxy(sockfd, "fcntl", arg_list, recv_buf, &err_val);

  printf("Returning from fcntl.\n");
  fflush(stdout);

  if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;

  
}





int getsockopt(int sockfd, int level, int option_name,
	       void *option_value, socklen_t *option_len)
{
  printf("Got here3");
  fflush(stdout);
  char arg_list[20] = "";
  char buf[10] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(level, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(option_name, buf, 10);
  strcat(arg_list, buf);

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "getsockopt", arg_list, recv_buf, &err_val);

  if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;

}





int setsockopt(int sockfd, int level, int option_name, const void *option_value, socklen_t option_len)
{
  printf("Got here4");
  fflush(stdout);
  char arg_list[30] = "";
  char buf[10] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(level, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");
  
  memset(buf, 0, strlen(buf));
  my_itoa(option_name, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  strcat(arg_list, (char*) option_value);
  strcat(arg_list, ",");

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "setsockopt", arg_list, recv_buf, &err_val);

  if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;

}



// ##################### MISCELLANEOUS CALLS ##############################

//int getpeername(int sockfd, struct sockaddr *address,
//		socklen_t *address_len)
//{
//
//  char arg_list[8] = "";
//  char buf[8] = "";
//
//  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];
//
//  memset(arg_list, 0, strlen(arg_list));
//  memset(buf, 0, strlen(buf));
//  my_itoa(repy_sock_fd, buf, 10);
//  strcat(arg_list, buf);
//
//  char recv_buf[RECV_SIZE];
//  char return_val[10];
//  int err_val;
//
//  // Send the info to the Repy proxy server
//  forward_api_to_proxy(sockfd, "getpeername", arg_list, recv_buf, &err_val);
//
//  if (err_val < 0) {
//    deserialize_sockaddr(address, recv_buf, return_val);
//    return atoi(return_val);
//  }
//  else
//    return -1; 
//
//}
//
//
//
//int getsockname(int sockfd, struct sockaddr *address,
//		socklen_t *address_len)
//{
//  char arg_list[8] = "";
//  char buf[8] = "";
//
//  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];
//
//  memset(arg_list, 0, strlen(arg_list));
//  memset(buf, 0, strlen(buf));
//  my_itoa(repy_sock_fd, buf, 10);
//  strcat(arg_list, buf);
//
//  char recv_buf[RECV_SIZE];
//  char return_val[10];
//  int err_val;
//
//  // Send the info to the Repy proxy server
//  forward_api_to_proxy(sockfd, "getsockname", arg_list, recv_buf, &err_val);
//
//  if (err_val < 0){
//    deserialize_sockaddr(address, recv_buf, return_val);
//    return atoi(return_val);
//  } 
//  else
//    return -1;
//}





int select(int nfds, fd_set *readfds, fd_set *writefds, 
	   fd_set *errorfds, struct timeval *timeout)
{
  printf("Got into select\n");
  fflush(stdout);
  char arg_list[1024] = "";
  char buf[8] = "";

  // Temporary solution, return everything as being ready.
  int i;
  int fd_ready = 0;
  return nfds;
  printf("About to forloop", i);
  fflush(stdout);  
  for (i=1; i <= nfds; i++)
  {
    printf("checking set for %d", i);
    fflush(stdout);
    if (FD_ISSET(i, readfds)){
      fd_ready += 1;
      printf("FD_ISSET for %d", i);
      fflush(stdout);
    }
  }
  printf("Got into select2\n");
  fflush(stdout);
  return fd_ready;
  
  /*
  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));

  my_itoa(nfds, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  // Serialize the readable file descriptors.
  serialize_fdset(nfds, writefds, arg_list);
  strcat(arg_list, ",");

  // Serialize the writable file descriptors.
  serialize_fdset(nfds, readfds, arg_list);
  strcat(arg_list, ",");

  // Serialize the error file descriptors.
  serialize_fdset(nfds, errorfds, arg_list);
  strcat(arg_list, ",");


  char recv_buf[RECV_SIZE];
  char return_val[10];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "select", arg_list, recv_buf, &err_val);

  // If there was no error, then we deserialize everything and return
  // the appropriate value.
  if (err_val < 0){
    deserialize_select(readfds, writefds, errorfds, recv_buf, return_val);
    return atoi(return_val);
  } 
  else
    return -1;
  */
}



int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{

  printf("Got into poll\n");
  fflush(stdout);
  char arg_list[1024] = "";
  char buf[8] = "";

  // Temporary solution, return everything as being ready.
  int i;
  int fd_ready = 0;
  printf("Returning from Poll");
  fflush(stdout);
  return 1;
    /*
  for (i=0; i <= nfds; i++)
  {
    if (FD_ISSET(i, fds))
      fd_ready += 1;
  }
    */
  //return fd_ready;
}

// ##################### CLOSE CALLS ##############################3

int shutdown(int sockfd, int how)
{
  printf("Got into shutdown\n");
  fflush(stdout);
  char arg_list[10] = "";
  char buf[10] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);
  strcat(arg_list, ",");

  memset(buf, 0, strlen(buf));
  my_itoa(how, buf, 10);
  strcat(arg_list, buf);

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "shutdown", arg_list, recv_buf, &err_val);

  if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;	
}




int close(int sockfd)
{
  printf("Got into close\n");
  fflush(stdout);

  char arg_list[10] = "";
  char buf[10] = "";

  int repy_sock_fd = socket_fd_dict[sockfd % MAX_SOCK_FD];

  memset(arg_list, 0, strlen(arg_list));
  memset(buf, 0, strlen(buf));
  my_itoa(repy_sock_fd, buf, 10);
  strcat(arg_list, buf);

  char recv_buf[RECV_SIZE];
  int err_val;

  // Send the info to the Repy proxy server
  forward_api_to_proxy(sockfd, "close", arg_list, recv_buf, &err_val);

  if (err_val == ERRBADFD)
    return (*libc_close)(sockfd);
  else if (err_val < 0)
    return atoi(recv_buf); 
  else
    return -1;	
}



// ===================== Serializing Functions =============================

void serialize_sockaddr(struct sockaddr* address, char* result_buf)
{
  char buf[20] = "";

  struct sockaddr_in* tmp_addr = (struct sockaddr_in*) address;

  /* Retrieve the ip address from the sockaddr structure and
   * convert it to human readable ip address.
   */
  char tmp_ip[20];
  int ipAddr = tmp_addr->sin_addr.s_addr;

  memset(buf, 0, strlen(buf));
  inet_ntop(AF_INET, &ipAddr, tmp_ip, 20);
  strcat(result_buf, tmp_ip);
  strcat(result_buf, ",");

  /* Retrieve and append the port of the address. */
  memset(buf, 0, strlen(buf));

  my_itoa((int)ntohs(tmp_addr->sin_port), buf, 10);
  strcat(result_buf, buf);    
}



/*
void serialize_fdset(int nfds, fd_set* file_set, char* result_buf)
{
  char buf[8] = "";
  int fd_val;

   
   // We check all the fd values to check if it is in the
   // the set of provided sockets. 
    
  for (fd_val=1; fd_val < nfds+1; fd_val++) {
    if (FD_ISSET(fd_val, file_set)) 
    {
      memset(buf, 0, strlen(buf));
      my_itoa(fd_val, buf, 10);
      strcat(result_buf, buf);
      strcat(result_buf, "#");
    }
  }

  // Check the last fd value. We do this outside of the for loop
  // in order to not get an extra '#' mark.
  fd_val += 1;
  if (FD_ISSET(fd_val, readfds)) 
  {
    memset(buf, 0, strlen(buf));
    my_itoa(fd_val, buf, 10);
    strcat(result_buf, buf);
  }


}
*/

// ============================= Deserialize Function =====================

void deserialize_proxy_msg(PROXY_REPLY* replystruct, char* result_buffer, int* error_value)
{
  strcpy(result_buffer, replystruct->response);
  *error_value = replystruct->err_val;
  return;
}

void deserialize_sockaddr(struct sockaddr* address, char *recv_buf, char *msg_buf)
{
  struct sockaddr_in remote_addr;
  char remoteip[16];
  char buf[10];
  int remoteport;
  int start_index = 0;
  int cur_index = 0;

  memset(remoteip, 0, strlen(remoteip));
  memset(buf, 0, strlen(buf));
  
  /* Find the location of where the IP address ends. */
  for (cur_index; cur_index < strlen(recv_buf); cur_index++){
    if (recv_buf[cur_index] == ',')
      break;
  }

  /* Copy over the IP address. */
  strncpy(remoteip, recv_buf + start_index, cur_index - start_index);

  /* Add the null character at the end. */
  remoteip[cur_index - start_index] = '\0';

  start_index = cur_index + 1;
  cur_index = start_index;
 
  /* Find the location of the port number in the recv buf. */
  for (cur_index; cur_index < strlen(recv_buf); cur_index++){
    if (recv_buf[cur_index] == ',')
      break;
  }
  
  /* Retrieve and convert the remote port number. */
  strncpy(buf, recv_buf + start_index, cur_index - start_index);

  /* Add the null character at the end. */
  buf[cur_index - start_index];
  remoteport = atoi(buf);

  /* Extract the message if there is any. */
  start_index = cur_index + 1;
  strncpy(msg_buf, recv_buf + start_index, strlen(recv_buf) - start_index);

  /* Add the null character at the end. */
  msg_buf[strlen(recv_buf) - start_index] = '\0';


  /* Generate the address. */
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_addr.s_addr = inet_addr(remoteip);
  remote_addr.sin_port = remoteport;


  /* Assign the remote address. */
  address = (struct sockaddr *) &remote_addr;

  return;
}

/*
void deserialize_select(fd_set* readfds, fd_set* writefds, fd_set* errorfds, char* recv_buf, int* return_val)
{
 
  // This function is used to deserialize the result for the select
  // call.
  

  int buf_len;
  char ptr[100];
  char buf[10];
  char readfd_str[100];
  char writefd_str[100];
  char errorfd_str[100];


  // Find the index of the firs ',', so we can figure out the return value.
  ptr = strchr(recv_buf, ',');
  buf_len = ptr - recv_buf;

  // Now we know the index where the return val ends, so we can copy it out.
  strncpy(buf, recv_buf, buf_len);
  *return_val = atoi(buf);  

  // Here we ignore the first part of recv_buf. Now we find the readfds that
  // were set. We increment ptr by one character as to ignore the first character.
  recv_buf = ++ptr;
  ptr = strchr(recv_buf, ',');
  buf_len = ptr - recv_buf;

  // deserialize the fds for readfds
  strncpy(buf, recv_buf, buf_len);
  deserialize_fdset(buf, readfds);

  // Here we ignore the second part of recv_buf. Now we find the writefds that
  // were set.
  recv_buf = ++ptr;
  ptr = strchr(recv_buf, ',');
  buf_len = ptr - recv_buf;

  // deserialize the fds for readfds
  strncpy(buf, recv_buf, buf_len);
  deserialize_fdset(buf, writefds);

  // Here we ignore the third part of recv_buf. Now we find the errorfds that
  // were set.
  recv_buf = ++ptr;
  ptr = strchr(recv_buf, ',');
  buf_len = ptr - recv_buf;

  // deserialize the fds for readfds
  strncpy(buf, recv_buf, buf_len);
  deserialize_fdset(buf, errorfds);

    
  return return_val;
}




void deserialize_fdset(char* fd_string, fd_set* result_fdset)
{
  int buf_len;
  char buf[10];
  char ptr[100];

  // Zero out the fd set.
  FD_ZERO(result_fdset)

  // Extract all the file descriptors from the string.
  while (true)
  {
    ptr = strchr(fd_string, '#');

    // If the pointer is null then we break out of the while loop.
    if (!ptr)
      break;
    
    // Extract the fd number.
    buf_len = ptr - fd_string;
    strncpy(buf, fd_string, buf_len);
    
    // Set the fd.
    FD_SET(atoi(buf), result_fdset);
  }
  
  // Set the last fd, since we broke from the while loop with the last
  // fd still remaining. The rest of the fd_string should just be a number.
  FD_SET(atoi(fd_string, result_fdset);
}
*/
