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
#define PORT 8080
#define SA struct sockaddr
#define DEVICE "lightbulb"
int brightness = 50;
enum Colors
{
	WHITE = 1,
	RED = 2,
	ORANGE = 3,
	YELLOW = 4,
	GREEN = 5,
	BLUE = 6,
	INDIGO = 7,
	VIOLET = 8
} currentColor = 1;
int sockfd;
pid_t childpid;

void reset_XMLsettings();
int set_XMLsettings(int brightness, int color);
void get_XMLsettings()
{

	FILE *fp;
	mxml_node_t *tree;
	mxml_node_t *node;
	if ((fp = fopen("2.xml", "r")) == NULL) {
		perror("FOPEN XML:");
		set_XMLsettings(50, 1);
		get_XMLsettings();
		return;
	}
	else {
		tree = mxmlLoadFile(NULL, fp, MXML_OPAQUE_CALLBACK);
		fclose(fp);
		node = mxmlGetFirstChild(mxmlGetFirstChild(tree)); // bightness is the node

		while (node != NULL)
		{
			if (strstr(mxmlGetElement(node), "brightness") != NULL)
			{
				printf("Brightness: %s\n", mxmlGetOpaque(mxmlGetFirstChild(node)));
				int possibleBrightness = atoi(mxmlGetOpaque(mxmlGetFirstChild(node)));
				if (possibleBrightness < 0 || possibleBrightness > 100)
				{
					reset_XMLsettings();
					brightness = 50;
				}
				else
					brightness = possibleBrightness;
			}
			else if (strstr(mxmlGetElement(node), "color") != NULL)
			{
				printf("Color: %s\n", mxmlGetOpaque(mxmlGetFirstChild(node)));
				int possibleColor = atoi(mxmlGetOpaque(mxmlGetFirstChild(node)));
				if (possibleColor <= 0 || possibleColor > 9)
				{
					reset_XMLsettings;
					possibleColor = 1;
				}
				else
					currentColor = possibleColor;
			}
			node = mxmlGetNextSibling(node);
		}
		printf("\n(After parsing): Brightness: %d | Color: %d\n", brightness, currentColor);
	}
}

void reset_XMLsettings()
{
	set_XMLsettings(50, 1);
}

int set_XMLsettings(int localbrightness, int color) {
	printf("\nSETTING XML: brightness:%d,color:%d\n", localbrightness, color);
	FILE *fp;
	char brightness_string[10];
	char color_string[10];
	sprintf(brightness_string, "%d", brightness);
	sprintf(color_string, "%d", color);

	if ((fp = fopen("2.xml", "w")) == NULL)
		perror("Reset Settings: open file error: ");

	mxml_node_t *xml;  /* <?xml ... ?> */
	mxml_node_t *node; /* <node> */
	mxml_node_t *group;

	xml = mxmlNewXML("1.0");
	group = mxmlNewElement(xml, "settings");
	node = mxmlNewText(xml, 12, color_string);
	node = mxmlNewElement(group, "brightness");
	mxmlNewText(node, 0, brightness_string);
	node = mxmlNewElement(group, "color");
	mxmlNewText(node, 0, color_string);
	mxmlSaveFile(xml, fp, MXML_NO_CALLBACK);
	fclose(fp);
	return 0;
}

char *change_setting(char *setting);

void func(int socketfd) // 1 - wait for another connection
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
		read(socketfd, buff, sizeof(buff));
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
		else {
			if (strstr(buff, "disconnect") != NULL)
				response = strdup("lightbulb:success@disconnected");
			else
			{
				printf("\n Exiting the program...\n");
				write(socketfd, "lightbulb@turnoff", sizeof("lightbulb@turnoff"));
				char readnull[3];
				read(socketfd, readnull, sizeof(readnull));
				read(socketfd, readnull, sizeof(readnull));
				exit(0);
			}
		}
		/////			COMMAND HANDLER 	////
		printf("Change_setting response: %s\t length: %lu\n", response, strlen(response));
		// and send that buffer to client
		int n;
		if (n = write(socketfd, response, strlen(response)) == -1)
			printf("writing failed");

		if (response != NULL)
			free(response);
	}

	printf("Closing socket...");
	close(sockfd);
}
char *change_setting(char *setting)
{ // DUPA @ CE E DE AFISAT
	char *response = NULL;
	//asta
	get_XMLsettings();
	//
	if (strncmp(setting, "light+", 7) == 0) // LIGHT +
		if (brightness <= 99)
		{
			if (brightness <= 95)
				brightness += 5;
			else
				brightness = 100;

			char aux[100];
			sprintf(aux, "lightbulb:success:light+@%d", brightness);
			response = strdup(aux);
		}
		else
		{
			response = strdup("lightbulb:failed@The lightbulb is already at full brightness!");
		}
	else if (strncmp(setting, "light-", 7) == 0) // LIGHT -
		if (brightness >= 1)
		{
			if (brightness >= 5)
				brightness -= 5;
			else
				brightness = 0;
			char aux[100];
			sprintf(aux, "lightbulb:success:light-@%d", brightness);
			response = strdup(aux);
		}
		else
		{
			response = strdup("lightbulb:failed@The lightbulb is too dim already!");
		}
	else if (strncmp(setting, "setcolor ", 9) == 0) // SETCOLOR [COLOR INT 0-8]
	{
		int ok = 0;
		for (int i = 1; i <= 9; i++)
		{
			if (strchr(setting, '0' + i) != NULL)
			{
				char aux[100];
				sprintf(aux, "lightbulb:success:setColor@%d", i);
				currentColor = i;
				response = strdup(aux);
				ok = 1;
				break;
			}
		}
		if (ok == 0)
			response = strdup("lightbulb:failed@The color was not found!");
	}
	else if (strncmp(setting, "setbrightness ", 9) == 0) // SETBRIGHTNESS [BRIGHTNESS INT 0-100]
	{
		char *brightnessChar = strtok(setting, " ");
		brightnessChar = strtok(NULL, " ");
		if (brightnessChar != NULL)
		{
			int brightnessInt = atoi(brightnessChar);
			brightness = brightnessInt;
			printf("\n brightness (int) : %d \n", brightnessInt);
			if (brightnessInt >= 0 && brightnessInt <= 100)
			{
				char aux[100];
				sprintf(aux, "lightbulb:success:setBrightness@%d", brightnessInt);
				printf("\n actual brightness: %d\n", brightness);
				response = strdup(aux);
			}
			else
				response = strdup("lightbulb:failed@The brightness bust be an integer between 0 and 100!");
		}
		else
		{
			response = strdup("lightbulb:failed@The brightness bust be an integer between 0 and 100!");
		}
	}
	else if (strncmp(setting, "exit", 9) == 0)
		response = strdup("EXIT");

	if (response == NULL)
		response = strdup("ERROR");
	else if (strstr(response, "success") != NULL)
		set_XMLsettings(brightness, currentColor);
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
	if (sockfd < 0)
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
	if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0) {
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

// ------ Until here everything is the same

////// THIS SHOULD BE IN ANOTHER WHILE WITHOUT THE WHILE IN MAIN		
	len = sizeof(cli);
	// Accept the data packet from client and verification
	while(1) {
	connfd = accept(sockfd, (SA *)&cli, &len);
	if (connfd < 0)
	{
		perror("server accept failed...\n");
		exit(0);
	}
	else { 
		if((childpid = fork()) == 0) { 
		close(sockfd);
		get_XMLsettings();
		printf("server accept the client...\n");
		printf("sending values...\n");
		char config[MAX];
		sprintf(config, "lightbulb:%d:%d", brightness, currentColor);
		if (write(connfd, config, strlen(config)) == -1)
			printf("writing failed");
		else
			printf("data sent\n");
		func(connfd);
		}
		
	}
	}
////////////////////////////////////////////////////////////
}
// Driver function
int main()
{
	get_XMLsettings();
		commSession();
}
