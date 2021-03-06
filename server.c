/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue will hold

int send_to_new_fd(int new_fd);


void sigchld_handler(int s)
{
	/*
		Remember that errno is a global int that is modified by functions whenever
		an error occurs. With that in mind, we don't want the waitpid() function to
		overwrite our errno, and we aren't going to analyze it for any useful data
		within this particular function, and so we'll simply save and restore it.
	*/
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	/*
		If it turns out that sa_family within the struct sa equals AF_INET, that means
		we are dealing with an IPv4 address. With that in mind, we'll simply return the
		IPv4 address that corresponds to the value we had passed. Note that we have to
		cast the data types appropriately.
	*/
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	/*
		Otherwise, this is actually an AF_INET6 address, or IPv6. With that in mind, we'll
		cast sockaddr *sa to a sockaddr_in6 * and then we'll pull the appropriate data.
	*/
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	/*
		We're going to create two new file descriptors, represented as ints.
		The first one is called sockfd, and will be used to listen to new connections.
		The second one is new_fd, and will be assigned to whatever socket we create in order
		to communicate with the appropriate client.
	*/

	int sockfd, new_fd; // listen on sock_fd, new connection on new_fd

	/*
		struct addrinfo hints will contain the socket configuration information, such as the ai_family,
		the ai_socktype, and the ai_flags. This will eventually be passed to getaddrinfo() so that 
	*/

	struct addrinfo hints, *servinfo, *p;


	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	
	// The variable yes is also quite interesting. Yes always equals one. Huh.
	int yes=1;

	// This variable is solely used to print out IP addresses when they are found. It's just a string, haha.
	char s[INET6_ADDRSTRLEN];

	// Um... okay?
	int rv;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(1);
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
 	}

 	freeaddrinfo(servinfo); // all done with this structure

 	if (p == NULL)
 	{
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
 	}

 	if (listen(sockfd, BACKLOG) == -1)
 	{
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
 	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
 		exit(1);
 	}

 	printf("server: waiting for connections...\n");

 	while(1)
 	{ // main accept() loop

 		/*
			Remember that their_addr is of type sockaddr_storage, which effectively should be padded to be the same
			size as the typical sockaddr. Notice how we cast it to the appropriate pointer when we call it in the
			accept() function for int new_fd.
 		*/

		sin_size = sizeof(their_addr);

		/*
			The new_fd will be any number other than -1 when a connection is actually accepted. Note that in any other
			case, their_addr is never modified. We only care to update their_addr if it turns out that there's a, well,
			new address. Besides, before useful information is even used with it, it will simply be fork()'d off anyways.

			Note: from this point on, new_fd is the socket that points to the remote computer. We are connected!
		*/
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

		if (new_fd == -1)
		{
			/*
				perror() will return the following statement:printf("server: got connection from %s\n", s);
				accept: (whatever the error is).

				Note that the accept() method will modify errorno and the appropriate error message.
			*/

			perror("accept");
			continue;
		}
		
		/*
			If we have reached this point-- congrats, you have received a connection from somewhere!
			
			inet_ntop() is used to convert IPv4 and IPv6 addresses from binary to text form.
			It will save the resulting string into char[] s, and write sizeof(s) many characters.
		*/
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));

		// Prints out the IP address.
	 	printf("server: got connection from %s\n", s);

	 	if (!fork())
	 	{ // this is the child process
			close(sockfd); // child doesn't need the listener
			int i = 0;
			send_to_new_fd(new_fd);
			close(new_fd);
			exit(0);
		}
		close(new_fd); // parent doesn't need this
	}

	return 0;
}



int send_to_new_fd(int new_fd)
{
	char message[100];
	while (1)
	{
		memset(message,0,sizeof(message));
		fgets(message, 100, stdin);
		if (send(new_fd, message, 100, 0) == -1)
			perror("send");
	}

	return 0;
}











