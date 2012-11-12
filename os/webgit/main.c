#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "network.h"


// global variable; can't be avoided because
// of asynchronous signal interaction
int still_running = TRUE;
char *addrbuff[256];		//an array of addresses for logging
int sockbuff[256];		//an array of integers that will hold all sockets associated with strings that are waiting to be consumed
int sockout = 0;		//index for next sock to be consumated
pthread_mutex_t workmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t workcv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t othermutex = PTHREAD_MUTEX_INITIALIZER;		//mutex used for writing to weblog.txt

void *worker()
{
	while (TRUE)
	{
		pthread_mutex_lock(&workmutex); //lock
		while (sockbuff[sockout] == 0 && still_running)		//wait until there is something to consume
		{
			pthread_cond_wait(&workcv, &workmutex); //wait on workcv and release workmutex
		}
		pthread_mutex_unlock(&workmutex); //unlock
		if (still_running == FALSE)		
		{
			return NULL;
		}
		else
		{
			//Allocate some variables
			char cwd[512];
			getcwd(cwd, 256);
			char fixedreq[512];
			strcpy(fixedreq, cwd);	//we make a copy because we use cwd for the path to weblog.txt while fixedreq will be the manipulated path from getrequest
			strcat(cwd, "/");	//throw '/' on end of cwd so a filepath can be built off it
			char *tempbuffer = malloc(sizeof(char) * 256);	//will be used as buffer in getrequest
			char mode = 'r';
			char *fourohfour = "404";		//success code
			FILE *datafile;
			struct stat statbuff;
			char *response;
			char *get = "GET";
			//Locked section of code begins: needs to be locked b/c we are manipulating shared variables
			pthread_mutex_lock(&workmutex); //lock
			//copy socket and address from global arrays
			int workingsock = sockbuff[sockout];
			char *workingaddr = addrbuff[sockout];
			//clear arrays, and increment index
			free(addrbuff[sockout]);
			sockbuff[sockout] = 0;
			sockout = (sockout + 1)%256;
			pthread_mutex_unlock(&workmutex); //unlock
			//Locked section of code ends
			int success = getrequest(workingsock, tempbuffer, 256);
			//making a copy of tempbuffer, with a null terminater at the end so that stat will work
			char reqbuffer[strlen(tempbuffer) + 1];
			strncpy(reqbuffer, tempbuffer, strlen(tempbuffer));
			reqbuffer[strlen(tempbuffer)] = '\0';
			//concat current directory with result from getrequest
			if (success == 0)
			{
				if (reqbuffer[0] == '/')
				{
					strcat(fixedreq, reqbuffer);	
				}
				else
				{
					strcat(fixedreq, "/");
					strcat(fixedreq, reqbuffer);
				}

			}
			int status = stat(fixedreq, &statbuff);		//check if the filepath of fixedreq exists
			if (status == 0)
			{
				/////////format a correct HTTP 200 response//////////////
				int datasize = statbuff.st_size;
				char *databuffer = malloc(sizeof(char) * datasize);
				response = malloc(sizeof(char) * (100 + datasize));
				datafile = fopen(fixedreq, &mode);
				fgets(databuffer, datasize, datafile);
				fclose(datafile);
				sprintf(response, "HTTP/1.1 200 OK\r\nContent-type: text/html\r\nContent-length: %i\r\n\r\n%s\r\n", datasize, databuffer);
				fourohfour = "200";
				printf("Sending 200 response.\n");
				free(databuffer);
			}
			else if (status == -1)
			{
				response = "HTTP/1.1 404 Not Found\r\n\r\n";
				printf("Sending 404 response.\n");
			}
			int bites = senddata(workingsock, response, strlen(response));
	
			/////////////////////////////////////////////////////
			//////////////////////LOGGING////////////////////////
			////////////////////////////////////////////////////

			//convert the integer bites into a string, bytes
			char bytes[strlen(response)];
			sprintf(bytes, "%i", bites);
			char *successorfail = malloc(sizeof(char) * (strlen(fourohfour) + 1 + strlen(bytes))); //successorfail is just the success code concatted with response size
			sprintf(successorfail,"%s %s", fourohfour, bytes);
			// get timestamp
			time_t mytime;
			mytime = time(NULL);
			char *timestr = ctime(&mytime);
			timestr[24] = '\0';
			//log is the string we will write to weblog.txt
			char *log = malloc(sizeof(char)*(strlen(workingaddr) + strlen(timestr) + strlen(get) + strlen(tempbuffer) + strlen(successorfail) + 6));
			sprintf(log,"%s %s %s %s %s\r\n",workingaddr, timestr, get, tempbuffer, successorfail); //create log by sprintfing appropriate strings with spaces in between
			char *weblog = "weblog.txt";
			strcat(cwd, weblog);
			pthread_mutex_lock(&othermutex); //lock because we are editing weblog.txt
			int logfd = open(cwd, O_APPEND | O_CREAT | O_WRONLY, S_IRWXU);
			write(logfd, log, strlen(log));
			close(logfd);
			pthread_mutex_unlock(&othermutex); //unlock
			free(successorfail);
			free(response);
			free(tempbuffer);
			free(log);
		}
	}
}

void signal_handler(int sig) 
{
        still_running = FALSE;
}

void usage(const char *progname) 
{
        fprintf(stderr, "usage: %s [-p port] [-t numthreads]\n", progname);
        fprintf(stderr, "\tport number defaults to 3000 if not specified.\n");
        fprintf(stderr, "\tnumber of threads is 1 by default.\n");
        exit(0);
}

void runserver(int numthreads, unsigned short serverport) 
{
	//////////////////////////////////////
        // init stuff//
	//////////////////////////////////////
	
	pthread_mutex_init(&workmutex, NULL);
	pthread_mutex_init(&othermutex, NULL);
	pthread_cond_init(&workcv, NULL);
	int sockin = 0;			//index for the next socket produced
        /////////////////////////////////
        // create pool of threads here //
	/////////////////////////////////
	pthread_t threadpool[numthreads];
	int i = 0;
	for (;i<256;i++)
	{
		sockbuff[i]=0;
	}
	i=0;
	for (;i<numthreads;i++)
	{
		pthread_create(&threadpool[i], NULL, worker, NULL); //turn on all threads on worker function
	}

        int main_socket = prepare_server_socket(serverport);
        if (main_socket < 0) 
	{
                exit(-1);
        }
        signal(SIGINT, signal_handler);

        struct sockaddr_in client_address;
        socklen_t addr_len;
        fprintf(stderr, "Server listening on port %d.  Going into request loop.\n", serverport);
        while (still_running) 
        {
                struct pollfd pfd = {main_socket, POLLIN};
                int prv = poll(&pfd, 1, 10000);
                if (prv == 0) 
		{
                        continue;
                } 
		else if (prv < 0) 
		{
	                PRINT_ERROR("poll");
	                still_running = FALSE;
	                continue;
	        }
                addr_len = sizeof(client_address);
                memset(&client_address, 0, addr_len);
                int new_sock = accept(main_socket, (struct sockaddr *)&client_address, &addr_len);
                if (new_sock > 0)
		{
			fprintf(stderr, "Got connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

	                /////////////////////////////////////////////////////////////
        	        //Hand the connection off to one of the threads in the pool//
                	/////////////////////////////////////////////////////////////		
			 
			//store client address in global array so worker threads can access it
			char *straddress = inet_ntoa(client_address.sin_addr);
			int intport = ntohs(client_address.sin_port);
			pthread_mutex_lock(&workmutex); //lock
			sockbuff[sockin] = new_sock;
			addrbuff[sockin] = malloc(sizeof(int) + (sizeof(char) * (1 + strlen(straddress))));
			sprintf(addrbuff[sockin], "%s:%i", straddress, intport);
			pthread_mutex_unlock(&workmutex); //unlock
			pthread_cond_signal(&workcv); //wake up a worker thread
			sockin = (sockin + 1)%256;	//increment the index to store socks
                }
        }
	if (still_running == FALSE)
	{
		i = 0;
		pthread_cond_broadcast(&workcv);	//wake up every sleeping thread
		for (; i < numthreads ; i++)
		{
			pthread_join(threadpool[i], NULL); //join all threads
		}
	}
        fprintf(stderr, "Server shutting down.\n");
        close(main_socket);
}


int main(int argc, char **argv) 
{
        unsigned short port = 3000;
        int num_threads = 1;

        int c;
        while (-1 != (c = getopt(argc, argv, "hp:t:"))) 
	{
                switch(c) 
		{
                        case 'p':
                                port = atoi(optarg);
                                if (port < 1024) 
				{
                                        usage(argv[0]);
                                }
                                break;

                        case 't':
                                num_threads = atoi(optarg);
                                if (num_threads < 1) 
				{
                                        usage(argv[0]);
                                }
                                break;
                        case 'h':
                        default:
                                usage(argv[0]);
                                break;
                }
        }

        runserver(num_threads, port);
        
        fprintf(stderr, "Server done.\n");
        exit(0);
}
