#include <stdio.h>
#include <mxml.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#define MAX 80
#define PORT 8082
#define SA struct sockaddr
#define DEVICE "VACUUM"
int power;
int speed;
// POWER/SPEED
int sockfd;
pid_t childpid;
void reset_XMLsettings();
int set_XMLsettings(int power, int speed);
void get_XMLsettings()
{

	FILE *fp;
	mxml_node_t *tree;
	mxml_node_t *node;
	if ((fp = fopen("vacuum.xml", "r")) == NULL)
	{
		perror("FOPEN XML:");
		set_XMLsettings(50, 50);
		get_XMLsettings();
		return;
	}
	else
	{
		tree = mxmlLoadFile(NULL, fp, MXML_OPAQUE_CALLBACK);
		fclose(fp);
		node = mxmlGetFirstChild(mxmlGetFirstChild(tree)); // brightness is the node

		while (node != NULL)
		{
			if (strstr(mxmlGetElement(node), "power") != NULL)
			{
				printf("power: %s\n", mxmlGetOpaque(mxmlGetFirstChild(node)));
				int possiblePower = atoi(mxmlGetOpaque(mxmlGetFirstChild(node)));
				if (possiblePower < 0 || possiblePower > 100)
				{
					reset_XMLsettings();
					power = 50;
				}
				else
					power = possiblePower;
			}
			else if (strstr(mxmlGetElement(node), "speed") != NULL)
			{
				printf("Speed: %s\n", mxmlGetOpaque(mxmlGetFirstChild(node)));
				int possibleSpeed = atoi(mxmlGetOpaque(mxmlGetFirstChild(node)));
				if (possibleSpeed <= 0 || possibleSpeed > 100)
				{
					reset_XMLsettings();
					possibleSpeed = 50;
				}
				else
					speed = possibleSpeed;
			}

			node = mxmlGetNextSibling(node);
		}
		printf("\n(After parsing):  Power: %d | Speed: %d \n", power, speed);
	}
}

void reset_XMLsettings()
{
	printf("Starting to reset something in XML...\n");
	set_XMLsettings(50, 1);
}

int set_XMLsettings(int localpower, int localspeed)
{
	printf("\nSETTING XML: power:%d,speed:%d\n", localpower, localspeed);
	FILE *fp;
	char power_string[10];
	char speed_string[10];

	sprintf(power_string, "%d", localpower);
	sprintf(speed_string, "%d", localspeed);

	if ((fp = fopen("vacuum.xml", "w")) == NULL)
		perror("Reset Settings: open file error: ");

	mxml_node_t *xml;  /* <?xml ... ?> */
	mxml_node_t *node; /* <node> */
	mxml_node_t *group;

	xml = mxmlNewXML("1.0");
	group = mxmlNewElement(xml, "settings");
	node = mxmlNewElement(group, "power");
	mxmlNewText(node, 0, power_string);
	node = mxmlNewElement(group, "speed");
	mxmlNewText(node, 0, speed_string);
	mxmlSaveFile(xml, fp, MXML_NO_CALLBACK);
	fclose(fp);
	return 0;
}

char *change_setting(char *setting);

int func(int sockfd)
{
	char buff[MAX];
	int n;
	// infinite loop for chat
	int possibleEnd = 0;
	while (1)
	{
		printf("\n\n");
		char *response;

		bzero(buff, MAX);
		// read the message from client and copy it in buffer
		read(sockfd, buff, sizeof(buff));
		// print buffer which contains the client contents
		printf("From client: %s \t", buff);

		if (strlen(buff) == 0) /// possibleEnd -  when null - non null - null is happening, there is a bug - WATCH OUT FOR ERRORS
			if (possibleEnd)
			{
				printf("Double Null detected\n");
				break;
			}
			else
			{
				possibleEnd = 1;
				printf("\nNULL DETECTED - Possible disconnect \n");
				continue;
			}

		////// 			COMMAND HANDLER		////
		if (strstr(buff, "disconnect") == NULL && strstr(buff, "turnoff") == NULL)
			response = change_setting(buff);
		else
		{
			if (strstr(buff, "disconnect") != NULL)
				response = strdup("vacuum:success@disconnected");
			else
			{
				printf("\n Exiting the program...\n");
				write(sockfd, "vacuum@turnoff", sizeof("vacuum@turnoff"));
				char readnull[3];
				read(sockfd, readnull, sizeof(readnull));
				read(sockfd, readnull, sizeof(readnull));
				exit(0);
			}
		}
		/////			COMMAND HANDLER 	////
		printf("What is sent to client: %s\t length: %lu\n", response, strlen(response));
		// and send that buffer to client
		int n;
		if (n = write(sockfd, response, strlen(response)) == -1)
			printf("writing failed");

		if (response != NULL)
			free(response);
	}
	return 1;
}
char *change_setting(char *setting)
{ // DUPA @ CE E DE AFISAT
	char *response = NULL;
	if (strncmp(setting, "setpower ", 9) == 0) // SETVOLUME [VOLUME INT 0-100]
	{
		char *powerChar = strtok(setting, " ");
		powerChar = strtok(NULL, " ");
		if (powerChar != NULL)
		{
			int powerInt = atoi(powerChar);
			power = powerInt;
			printf("\n power (int) : %d \n", powerInt);
			if (powerInt >= 0 && powerInt <= 100)
			{
				char aux[100];
				sprintf(aux, "vacuum:success:setPower@%d", powerInt);
				printf("\n actual Power: %d\n", power);
				response = strdup(aux);
			}
			else
				response = strdup("vacuum:failed@The power must be an integer between 0 and 100!");
		}
		else
		{
			response = strdup("tv:failed@The power must be an integer between 0 and 100!");
		}
	}
	else if (strncmp(setting, "setspeed ", 9) == 0) // SETBRIGHTNESS [BRIGHTNESS INT 0-100]
	{
		char *speedChar = strtok(setting, " ");
		speedChar = strtok(NULL, " ");
		if (speedChar != NULL)
		{
			int speedInt = atoi(speedChar);
			speed = speedInt;
			printf("\n speed (int) : %d \n", speedInt);
			if (speedInt >= 0 && speedInt <= 2)
			{
				char aux[100];
				sprintf(aux, "vacuum:success:setSpeed@%d", speedInt);
				printf("\n actual Speed: %d\n", speed);
				response = strdup(aux);
			}
			else
				response = strdup("Vacuum:failed@The speed must be an integer between 0 and 2!");
		}
		else
		{
			response = strdup("tv:failed@The speed bust be an integer between 0 and 100!");
		}
	}
	else if (strncmp(setting, "exit", 9) == 0)
		response = strdup("EXIT");

	if (response == NULL)
		response = strdup("ERROR");
	else if (strstr(response, "success") != NULL)
		set_XMLsettings(power, speed);
	return response;
}

int commSession() // 1 - close the device, 0 - wait another connection
{
	printf("\n\n\n");
	printf("Started Communication session\n");
	int connfd, len;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// socket options

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
	{
		perror("socket bind failed...");
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");
	// Now server is ready to listen and verification
	printf("Started listening (again)\n");
	if ((listen(sockfd, 5)) != 0)
	{
		perror("Listen failed...\n");
		exit(0);
	}
	else
		printf("Server listening..\n");
	len = sizeof(cli);
	// Accept the data packet from client and verification
	while (1)
	{
		connfd = accept(sockfd, (SA *)&cli, &len);
		if (connfd < 0)
		{
			perror("server accept failed...\n");
			exit(0);
		}
		else
		{
			if ((childpid = fork()) == 0) {
				close(sockfd);
				get_XMLsettings();
				printf("server accept the client...\n");
				printf("sending values...\n");
				char config[MAX];
				sprintf(config, "vacuum:%d:%d", power, speed);
				if (write(connfd, config, strlen(config)) == -1)
					printf("writing failed");
				else
					printf("data sent\n");
				func(connfd);
			}
		}
	}

	// Function for chatting between client and server 2 - needs exit 1 - keep listening
	int option;
	if (option = func(connfd) == 2)
	{
		printf("Server exit because func returned 2");
		exit(0);
	}

	printf("Closing socket...");
	// After chatting close the socket
	close(sockfd);
}
// Driver function
int main()
{
	get_XMLsettings();
	commSession();
}
