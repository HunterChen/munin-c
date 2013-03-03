#include <libgen.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>


char VERSION[] = "1.0.0";
const int yes = 1; 

int verbose = 0;

char* host = "";
unsigned short port = 0;
char* plugin_dir = "plugins";
char* spoolfetch_dir = "";

int handle_connection();

int main(int argc, char *argv[]) {

	int optch;
	extern int opterr;
	int optarg_len;

	char format[] = "vd:h:s:p:";

	struct sockaddr_in server;
	struct sockaddr_in client;

	socklen_t client_len = sizeof(client);

	int sock_listen;
	int sock_accept;


	opterr = 1;

	while ((optch = getopt(argc, argv, format)) != -1)
	switch (optch) {
		case 'v':
			verbose ++;
			break;
		case 'd':
			optarg_len = strlen(optarg);
			plugin_dir = (char *) malloc(optarg_len + 1);
			strcpy(plugin_dir, optarg);
			break;
		case 'h':
			optarg_len = strlen(optarg);
			host = (char *) malloc(optarg_len + 1);
			strcpy(host, optarg);
			break;
		case 's':
			optarg_len = strlen(optarg);
			spoolfetch_dir = (char *) malloc(optarg_len + 1);
			strcpy(spoolfetch_dir, optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
	}

	/* get default hostname if not precised */
	if (! strlen(host)) {
		host = (char *) malloc(HOST_NAME_MAX + 1);
		gethostname(host, HOST_NAME_MAX);
	}

	if (! port) {
		/* use a 1-shot stdin/stdout */
		return handle_connection();
	}

	/* port is set, listen to this port and
           handle clients, one at a time */

	/* Get a socket for accepting connections. */
	if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return(2);
	}

	/* Bind the socket to the server address. */
	memset(&server, 0, sizeof(&server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;

	if (setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
		perror("setsockopt");
	}

	if (bind(sock_listen, (struct sockaddr*) &server, sizeof(server)) < 0) {
		return(3);
	}

	/* Listen for connections. Specify the backlog as 1. */
	if (listen(sock_listen, 1) != 0) {
		return(4);
	}

	/* Accept a connection. */
	while ((sock_accept = accept(sock_listen, (struct sockaddr*) &client, &client_len)) != -1) { 
		/* connect the accept socket to stdio */
		dup2(sock_accept, 0);
		dup2(sock_accept, 1);

		/* close socket after dup() */
		close(sock_accept);

		stdin = fdopen(0, "r");
		stdout = fdopen(1, "w");

		if (handle_connection()) break;
	}

	return 5;
}

int handle_connection() {
	char line[LINE_MAX];
	char output[4096];

	printf("# munin node at %s\n", host);
	while (fgets(line, LINE_MAX, stdin) != NULL) {
		char* cmd;
		char* arg;

		line[LINE_MAX-1] = '\0';

		cmd = strtok(line, " \t\n");
		if(cmd == NULL)
			arg = NULL;
		else
			arg = strtok(NULL, " \t\n");

		if (!cmd || strlen(cmd) == 0) {
			printf("# empty cmd\n");
		} else if (strcmp(cmd, "version") == 0) {
			printf("munin c node version: %s\n", VERSION);
		} else if (strcmp(cmd, "nodes") == 0) {
			printf("%s\n", host);
			printf(".\n");
		} else if (strcmp(cmd, "quit") == 0) {
			return(0);
		} else if (strcmp(cmd, "list") == 0) {
			DIR* dirp = opendir(plugin_dir);
			struct dirent* dp;
			while ((dp = readdir(dirp)) != NULL) {
				char cmdline[LINE_MAX];
				char* plugin_filename = dp->d_name;;

				if (plugin_filename[0] == '.') {
					/* No dotted plugin */
					continue;
				}

				snprintf(cmdline, LINE_MAX, "%s/%s", plugin_dir, plugin_filename);
				if (access(cmdline, X_OK) == 0) {
					printf("%s ", plugin_filename);
				}
			}
			printf("\n");
			closedir(dirp);
		} else if (
				strcmp(cmd, "config") == 0 ||
				strcmp(cmd, "fetch") == 0
			) {
			char cmdline[LINE_MAX];
			pid_t pid;
			if(arg == NULL) {
				printf("# no plugin given\n");
				continue;
			}
			if(arg[0] == '.' || strchr(arg, '/')) {
				printf("# invalid plugin character");
				continue;
			}
			snprintf(cmdline, LINE_MAX, "%s/%s", plugin_dir, arg);
			if (access(cmdline, X_OK) == -1) {
				printf("# unknown plugin: %s\n", arg);
				continue;
			}
			if(0 == (pid = vfork())) {
				execl(cmdline, arg, cmd, NULL);
				/* according to vfork(2) we must use _exit */
				_exit(1);
			} else if(pid < 0) {
				printf("# fork failed\n");
				continue;
			} else {
				waitpid(pid, NULL, 0);
			}
			printf(".\n");
		} else if (strcmp(cmd, "cap") == 0) {
			printf("cap ");
			if (strlen(spoolfetch_dir)) {
				printf("spool ");
			}
			printf("\n");
		} else if (strcmp(cmd, "spoolfetch") == 0) {
			printf("# not implem yet cmd: %s\n", cmd);
		} else {
			printf("# unknown cmd: %s\n", cmd);
		}
	}

	return 0;
}
