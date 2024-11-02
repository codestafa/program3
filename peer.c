#include <dirent.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * EECE 446
 * Mustafa Ali
 * Mohsen Amiri
 * Program 3
 * Fall 2024
 */

// Struct to deal with files
typedef struct {
  char **fileNames;
  int fileCount;
  int bitCount; // Total size of all file names
} FileList;

int lookup_and_connect(const char *host, const char *service);
FileList fileHandler();
void join(char joinMessage[], int id);
FileList publish(char publishMessage[], FileList files);
void search(char searchMessage[], char *searchCommand);

void searchLogic(int s, char message[100], char command[100],
                 char response[10]);
int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Usage: %s <host> <port> <id>\n", argv[0]);
    return 1;
  }

  int s; // the socket

  char userCommand[100];
  char searchCommand[100];

  // Argument variables
  char *host = argv[1];
  char *port = argv[2];
  int id = atoi(argv[3]);

  // Join
  char joinMessage[5];
  bool userJoined = false;

  // Publish
  FileList files = fileHandler();
  char publishMessage[files.bitCount +
                      5]; // 5 because the bitCount does not account for the
                          // previous bytes needed

  // Search
  char searchMessage[100];
  char searchResponse[10];

  if ((s = lookup_and_connect(host, port)) < 0) {
    exit(1);
  }

  while (1) {
    printf("Enter JOIN, PUBLISH, SEARCH, FETCH, or EXIT: \n");
    fgets(userCommand, sizeof(userCommand), stdin);
    userCommand[strcspn(userCommand, "\n")] = 0; // Remove newline

    // If statement to exit the program, we are closing the socket
    if (strcmp(userCommand, "EXIT") == 0 || strcmp(userCommand, "exit") == 0) {
      printf("Exiting...\n");
      close(s);
      break;
    }

    // If statement to join the registry, we only want to publish or search if
    // userJoined is true
    if ((strcmp(userCommand, "JOIN") == 0 ||
         strcmp(userCommand, "join") == 0) &&
        !userJoined) {
      join(joinMessage, id);
      if (send(s, joinMessage, sizeof(joinMessage), 0) == -1) {
        perror("send");
      } else {
        userJoined = true;
      }
    }

    // If statement to publish files into the registry
    if ((strcmp(userCommand, "PUBLISH") == 0 ||
         strcmp(userCommand, "publish") == 0) &&
        userJoined) {
      publish(publishMessage, files);
      if (send(s, publishMessage, sizeof(publishMessage), 0) == -1) {
        perror("send");
      }
    }

    // If statement to search a registry for the name of a specific file. We
    // receive the peer id, ip, and port
    if ((strcmp(userCommand, "SEARCH") == 0 ||
         strcmp(userCommand, "search") == 0) &&
        userJoined) {
      search(searchMessage, searchCommand);
      if (send(s, searchMessage, strlen(searchMessage) + 1, 0) == -1) {
        perror("send");
      } else {
        int recvIt = recv(s, searchResponse, sizeof(searchResponse), 0);
        if (recvIt > 0) {
          searchResponse[recvIt] = '\0';

          uint32_t peerID;
          uint32_t ipAddress;
          uint16_t port;

          memcpy(&peerID, &searchResponse[0], 4);
          memcpy(&ipAddress, &searchResponse[4], 4);
          memcpy(&port, &searchResponse[8], 2);

          peerID = ntohl(peerID);
          ipAddress = ntohl(ipAddress);
          port = ntohs(port);

          if (peerID != 0) {
            printf("File found at:\n");
            printf("  Peer %u\n", peerID);
            printf("  %u.%u.%u.%u", (ipAddress >> 24) & 0xFF,
                   (ipAddress >> 16) & 0xFF, (ipAddress >> 8) & 0xFF,
                   ipAddress & 0xFF);
            printf(":%u\n", port);
          } else {
            printf("  File not found.\n");
          }
        }
      }
    }

    if ((strcmp(userCommand, "FETCH") == 0 ||
         strcmp(userCommand, "fetch") == 0) &&
        !userJoined) {

      search(searchMessage, searchCommand);
      if (send(s, searchMessage, strlen(searchMessage) + 1, 0) == -1) {
        perror("send");
      } else {
        int peer_fd;
        int recvIt = recv(s, searchResponse, sizeof(searchResponse), 0);
        if (recvIt > 0) {
          searchResponse[recvIt] = '\0';

          uint32_t peerID;
          uint32_t ipAddress;
          uint16_t port;

          memcpy(&peerID, &searchResponse[0], 4);
          memcpy(&ipAddress, &searchResponse[4], 4);
          memcpy(&port, &searchResponse[8], 2);

          peerID = ntohl(peerID);
          ipAddress = ntohl(ipAddress);
          port = ntohs(port);

          char ipString[16];   // Enough to hold "255.255.255.255\0"
          char portString[6];  // Enough to hold "65535\0"

          // Format the IP address as a string
          snprintf(ipString, sizeof(ipString), "%u.%u.%u.%u",
                   (ipAddress >> 24) & 0xFF,
                   (ipAddress >> 16) & 0xFF,
                   (ipAddress >> 8) & 0xFF,
                   ipAddress & 0xFF);

          // Format the port as a string
          snprintf(portString, sizeof(portString), "%u", port);

          printf("Formatted IP Address: %s\n", ipString);
          printf("Formatted Port: %s\n", portString);

          printf("Formatted IP Address: %s\n", ipString);
          printf("Formatted Port: %s\n", portString);


          if ((peer_fd = lookup_and_connect(ipString,portString)) < 0) {
            exit(1);
          }


          if (peerID != 0) {
            printf("File found at:\n");
            printf("  Peer %u\n", peerID);
            printf("  %u.%u.%u.%u", (ipAddress >> 24) & 0xFF,
                   (ipAddress >> 16) & 0xFF, (ipAddress >> 8) & 0xFF,
                   ipAddress & 0xFF);
            printf(":%u\n", port);
          } else {
            printf("  File not found.\n");
          }
        }
      }
    }
    }

  close(s);
  return 0;
}

int lookup_and_connect(const char *host, const char *service) {
  struct addrinfo hints;
  struct addrinfo *rp, *result;
  int s;

  /* Translate host name into peer's IP address */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  if ((s = getaddrinfo(host, service, &hints, &result)) != 0) {
    fprintf(stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }

  /* Iterate through the address list and try to connect */
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
      continue;
    }

    if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }
  }
  if (rp == NULL) {
    perror("stream-talk-client: connect");
    return -1;
  }
  freeaddrinfo(result);

  return s;
}

// Function that the handles the file logic. It counts the number of files,
// counts the total bits, and gets file names
FileList fileHandler() {
  DIR *dir;
  struct dirent *entry;
  char *dirName = "./SharedFiles";
  int count = 0;
  int maxFiles = 100;
  char **charArr = malloc(
      maxFiles * sizeof(char *)); // Allocate memory for the array of strings

  if (!charArr) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  dir = opendir(dirName);
  if (!dir) {
    perror("opendir");
    exit(EXIT_FAILURE);
  }

  // Loop through directory entries
  while ((entry = readdir(dir)) != NULL && count < maxFiles) {
    if (entry->d_type == DT_REG) {        // Only regular files
      size_t len = strlen(entry->d_name); // Get the length of the filename
      charArr[count] = malloc(len + 1);
      if (!charArr[count]) {
        perror("malloc");
        closedir(dir);
        exit(EXIT_FAILURE);
      }
      strcpy(charArr[count],
             entry->d_name); // Copy the filename into charArr[count]
      count++;               // Increment file count
    }
  }
  unsigned int bitCount = 0;
  for (int i = 0; i < count; i++) {
    bitCount += strlen(charArr[i]); // Counting the total bits
  }

  bitCount += count; // Add to total bitCount
  closedir(dir);

  FileList result;
  result.fileNames = charArr;
  result.fileCount = count;
  result.bitCount = bitCount;
  return result;
}

// Function for joining a registry
void join(char joinMessage[], int id) {
  char actionByte = 0;
  memcpy(joinMessage, &actionByte, 1);
  uint32_t net_id = htonl(id);
  memcpy(joinMessage + 1, &net_id, 4);
}

// Function for publishing files to a registry
FileList publish(char publishMessage[], FileList files) {
  char actionByte = 1;
  memcpy(publishMessage, &actionByte, 1);
  uint32_t fileCount_htonl = htonl(files.fileCount);
  memcpy(publishMessage + 1, &fileCount_htonl, 4);

  int offset = 5;
  for (int i = 0; i < files.fileCount; i++) {
    int nameLength = strlen(files.fileNames[i]) + 1;
    memcpy(publishMessage + offset, files.fileNames[i], nameLength);
    offset += nameLength;
  }
  return files;
}

// Function to search a registry for a file name
void search(char searchMessage[], char *searchCommand) {
  char actionByte = 2;
  memcpy(searchMessage, &actionByte, 1);
  printf("Enter file name to search: ");
  fgets(searchCommand, 100, stdin);
  searchCommand[strcspn(searchCommand, "\n")] = 0;

  memcpy(searchMessage + 1, searchCommand, strlen(searchCommand) + 1);
}
