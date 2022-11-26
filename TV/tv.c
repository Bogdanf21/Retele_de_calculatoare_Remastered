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
#define PORT 8081
#define SA struct sockaddr
#define DEVICE "TV"
int brightness = 50;
int volume = 50;
int channel = 1;
// BRIGHTNESS/VOLUME/CHANNEL
int sockfd;
pid_t childpid;

void reset_XMLsettings();
int set_XMLsettings(int brightness, int volume, int channel);
void get_XMLsettings()
{

	FILE *fp;
	mxml_node_t *tree;
	mxml_node_t *node;
	if ((fp = fopen("tv.xml", "r")) == NULL)
	{
		perror("FOPEN XML:");
		set_XMLsettings(50, 50, 1);
		get_XMLsettings();
		return;
	}
	else
	{
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
			else if (strstr(mxmlGetElement(node), "volume") != NULL)
			{
				printf("Volume: %s\n", mxmlGetOpaque(mxmlGetFirstChild(node)));
				int possibleVolume = atoi(mxmlGetOpaque(mxmlGetFirstChild(node)));
				if (possibleVolume <= 0 || possibleVolume > 100)
				{
					reset_XMLsettings();
					possibleVolume = 50;
				}
				else
					volume = possibleVolume;
			}
			else if (strstr(mxmlGetElement(node), "channel") != NULL)
			{
				printf("Channel: %s\n", mxmlGetOpaque(mxmlGetFirstChild(node)));
				int possibleChannel = atoi(mxmlGetOpaque(mxmlGetFirstChild(node)));
				if (possibleChannel <= 0 || possibleChannel > 1000)
				{
					reset_XMLsettings();
					possibleChannel = 50;
				}
				else
					channel = possibleChannel;
			}
			node = mxmlGetNextSibling(node);
		}
		printf("\n(After parsing): Brightness: %d | Volume: %d | Channel: %d\n", brightness, volume, channel);
	}
}

void reset_XMLsettings()
{
	printf("Starting to reset something in XML...\n");
	set_XMLsettings(50, 50, 1);
}

int set_XMLsettings(int localbrightness, int localvolume, int localchannel)
{
	printf("\nSETTING XML: brightness:%d,volume:%d,channel:%d\n", localbrightness, localvolume, localchannel);
	FILE *fp;
	char brightness_string[10];
	char volume_string[10];
	char channel_string[10];
	sprintf(brightness_string, "%d", brightness);
	sprintf(volume_string, "%d", volume);
	sprintf(channel_string, "%d", channel);

	if ((fp = fopen("tv.xml", "w")) == NULL)
		perror("Reset Settings: open file error: ");

	mxml_node_t *xml;  /* <?xml ... ?> */
	mxml_node_t *node; /* <node> */
	mxml_node_t *group;

	xml = mxmlNewXML("1.0");
	group = mxmlNewElement(xml, "settings");
	node = mxmlNewElement(group, "brightness");
	mxmlNewText(node, 0, brightness_string);
	node = mxmlNewElement(group, "volume");
	mxmlNewText(node, 0, volume_string);
	node = mxmlNewElement(group, "channel");
	mxmlNewText(node, 0, channel_string);
	mxmlSaveFile(xml, fp, MXML_NO_CALLBACK);
	fclose(fp);
	return 0;
}

char *change_setting(char *setting);

int func(int sockfd) // 1 - wait for another connection
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
				response = strdup("tv:success@disconnected");
			else
			{
				printf("\n Exiting the program...\n");
				write(sockfd, "tv@turnoff", sizeof("tv@turnoff"));
				char readnull[3];
				read(sockfd, readnull, sizeof(readnull));
				read(sockfd, readnull, sizeof(readnull));
				exit(0);
			}
		}
		/////			COMMAND HANDLER 	////
		printf("Change_setting response: %s\t length: %lu\n", response, strlen(response));
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
	if (strncmp(setting, "setvolume ", 9) == 0) // SETVOLUME [VOLUME INT 0-100]
	{
		char *volumeChar = strtok(setting, " ");
		volumeChar = strtok(NULL, " ");
		if (volumeChar != NULL)
		{
			int volumeInt = atoi(volumeChar);
			volume = volumeInt;
			printf("\n volume (int) : %d \n", volumeInt);
			if (volumeInt >= 0 && volumeInt <= 100)
			{
				char aux[100];
				sprintf(aux, "tv:success:setVolume@%d", volumeInt);
				printf("\n actual Volume: %d\n", volume);
				response = strdup(aux);
			}
			else
				response = strdup("tv:failed@The volume must be an integer between 0 and 100!");
		}
		else
		{
			response = strdup("tv:failed@The volume must be an integer between 0 and 100!");
		}
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
				sprintf(aux, "tv:success:setBrightness@%d", brightnessInt);
				printf("\n actual brightness: %d\n", brightness);
				response = strdup(aux);
			}
			else
				response = strdup("tv:failed@The brightness bust be an integer between 0 and 100!");
		}
		else
		{
			response = strdup("tv:failed@The brightness bust be an integer between 0 and 100!");
		}
	}
	else if (strncmp(setting, "setchannel ", 10) == 0) // SETCHANNEL [CHANNEL INT 1-1000]
	{
		char *channelChar = strtok(setting, " ");
		channelChar = strtok(NULL, " ");
		if (channelChar != NULL)
		{
			int channelInt = atoi(channelChar);
			channel = channelInt;
			printf("\n channel (int) : %d \n", channelInt);
			if (channelInt >= 1 && channelInt <= 1000)
			{
				char aux[100];
				sprintf(aux, "tv:success:setChannel@%d", channelInt);
				printf("\n actual channel: %d\n", channel);
				response = strdup(aux);
			}
			else
				response = strdup("tv:failed@The channel must be an integer between 1 and 1000!");
		}
		else
		{
			response = strdup("tv:failed@The channel must be an integer between 1 and 1000!");
		}
	}
	else if (strncmp(setting, "exit", 9) == 0)
		response = strdup("EXIT");

	if (response == NULL)
		response = strdup("ERROR");
	else if (strstr(response, "success") != NULL)
		set_XMLsettings(brightness, volume, channel);
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
// ------------------- Until here everything is the same
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
			sprintf(config, "tv:%d:%d:%d", brightness, volume, channel);
			if (write(connfd, config, strlen(config)) == -1)
				printf("writing failed");
			else
				printf("data sent\n");
			func(connfd);
		}
	}
	}
}
// Driver function
int main()
{
	get_XMLsettings();
	commSession();
}
