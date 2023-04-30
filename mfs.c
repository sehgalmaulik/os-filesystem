// Team X86 FTW 

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define WHITESPACE " \t\n" // We want to split our command line up into tokens
#define MAX_COMMAND_SIZE 255 // The maximum command-line size
#define MAX_NUM_ARGUMENTS 5 
#define FIRST_DATA_BLOCK 300

uint8_t data[NUM_BLOCKS][BLOCK_SIZE];

uint8_t free_blocks[65536];


// directory 

struct directoryEntry
{
  char filename[64];
  short in_use;
  int32_t inode;
};


// inode

struct inode
{
  short in_use;
  int32_t blocks[BLOCKS_PER_FILE];
  uint8_t attribute;
};

struct directoryEntry* directory;
struct inode* inodes;
FILE* fp;
char image_name[64];
uint8_t image_open;


//add all the functions here

void init()
{
  directory = (struct directoryEntry*)&data[0][0];
  inodes = (struct inode*)&data[20][0];

  memset(image_name, 0, 64);
  image_open = 0;


  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    directory[i].in_use = 0;
    directory[i].inode = -1;

    memset(directory[i].filename, 0, 64);

    int j;
    for (j = 0; j < NUM_BLOCKS; j++)
    {
      inodes[i].blocks[j] = -1;
      inodes[i].in_use = 0;
      inodes[i].attribute = 0;
    }
  
  }
  int j;
  for (j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }

  directory[0].in_use = 1;
  strncpy(directory[0].filename, "file.txt", strlen("file.txt"));

}

//creating function df
void df()
{
  int j;
  int count = 0;

  for (j = FIRST_DATA_BLOCK; j < NUM_BLOCKS; j++)
  {
    if (free_blocks[j])
    {
      count++;
    }
  }
  printf("%d bytes free\n", count * BLOCK_SIZE);
}

void createfs(char* filename)
{
  fp = fopen(filename, "w+");

  strncpy(image_name, filename, strlen(filename));

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);
  image_open = 1;


  fclose(fp);

}

// LIST
void list(int h, int a)
{
  int i;
  int not_found = 1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      if (h && (directory[i].filename[0] == '.' && directory[i].filename[1] != '\0'))
      {
        not_found = 0;
        printf("%s", directory[i].filename);

        if (a)
        {
          printf("\t");
          int j;
          for (j = 7; j >= 0; j--)
          {
            printf("%u", (inodes[directory[i].inode].attribute >> j) & 1);
          }
        }

        printf("\n");
      }
      else if (!h && (directory[i].filename[0] != '.' || directory[i].filename[1] == '\0'))
      {
        not_found = 0;
        printf("%s", directory[i].filename);

        if (a)
        {
          printf("\t");
          int j;
          for (j = 7; j >= 0; j--)
          {
            printf("%u", (inodes[directory[i].inode].attribute >> j) & 1);
          }
        }

        printf("\n");
      }
    }
  }

  if (not_found)
  {
    printf("ERROR: No files found.\n");
  }
}

// SAVE
void savefs()
{
  if (image_open == 0)
  {
    printf("ERROR: Disk image is not open.\n");
    return;
  }

  fp = fopen(image_name, "r+");
  if (fp == NULL)
  {
    printf("ERROR: Could not open file for writing.\n");
    return;
  }

  fwrite(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
  fflush(fp);
  memset(image_name, 0, 64);
  fclose(fp);
}

void openfs(char* filename)
{
  fp = fopen(filename, "r+");
  if (fp == NULL)
  {
    printf("ERROR: File not found.\n");
    return;
  }

  strncpy(image_name, filename, strlen(filename));

  fread(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
  image_open = 1;

  fclose(fp);

}



void readfile(char* filename, int starting_byte, int num_bytes)
{
  int i;
  int file_found = 0;

  int32_t file_inode = -1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      if (strncmp(directory[i].filename, filename, strlen(filename)) == 0)
      {
        file_found = 1;
        file_inode = directory[i].inode;
        break;
      }
    }
  }

  if (!file_found)
  {
    printf("ERROR: File not found\n");
    return;
  }

  if (starting_byte < 0 || starting_byte >= (BLOCKS_PER_FILE * BLOCK_SIZE))
  {
    printf("ERROR: Invalid starting byte.\n");
    return;
  }

  struct inode* inode_ptr = &inodes[file_inode];
  int file_size = inode_ptr->attribute;

  if (num_bytes <= 0 || starting_byte + num_bytes > file_size)
  {
    printf("ERROR: Invalid number of bytes.\n");
    return;
  }

  int block_index = starting_byte / BLOCK_SIZE;
  int block_offset = starting_byte % BLOCK_SIZE;
  int remaining_bytes = num_bytes;

  while (remaining_bytes > 0 && block_index < BLOCKS_PER_FILE)
  {
    int32_t block_num = inode_ptr->blocks[block_index];

    if (block_num == -1)
    {
      printf("ERROR: Invalid block number.\n");
      return;
    }

    int bytes_to_read = (remaining_bytes > (BLOCK_SIZE - block_offset)) ? BLOCK_SIZE - block_offset : remaining_bytes;

    for (i = 0; i < bytes_to_read; i++)
    {
      printf("%02x ", data[block_num][block_offset + i]);
    }

    remaining_bytes -= bytes_to_read;
    block_offset = 0;
    block_index++;
  }

  if (remaining_bytes > 0)
  {
    printf("ERROR: Could not read the entire specified range.\n");
  }
}

void closefs()
{
  if (image_open == 0)
  {
    printf("ERROR: Disk image is not open.\n");
    return;
  }
  fclose(fp);
  image_open = 0;
  memset(image_name, 0, 64);
}

void encrypt(char* filename, uint8_t cipher)
{
  size_t i; //changing from int to size_t as also used as index
  int file_found = 0;
  int32_t file_inode = -1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      if (strncmp(directory[i].filename, filename, strlen(filename)) == 0)
      {
        file_found = 1;
        file_inode = directory[i].inode;
        break;
      }
    }
  }

  if (!file_found)
  {
    printf("ERROR: File not found\n");
    return;
  }

  struct inode* inode_ptr = &inodes[file_inode];
  int file_size = 0;

  for (i = 0; i < BLOCKS_PER_FILE; i++)
  {
    if (inode_ptr->blocks[i] != -1)
    {
      file_size += BLOCK_SIZE;
    }
    else
    {
      break;
    }
  }

  uint8_t buffer[file_size];
  int buffer_index = 0;

  for (i = 0; i < BLOCKS_PER_FILE && inode_ptr->blocks[i] != -1; i++)
  {
    int32_t block_num = inode_ptr->blocks[i];
    memcpy(buffer + buffer_index, data[block_num], BLOCK_SIZE);
    buffer_index += BLOCK_SIZE;
  }

  for (i = 0; i < file_size; i++)
  {
    buffer[i] ^= cipher;
  }

  buffer_index = 0;

  for (i = 0; i < BLOCKS_PER_FILE && inode_ptr->blocks[i] != -1; i++)
  {
    int32_t block_num = inode_ptr->blocks[i];
    memcpy(data[block_num], buffer + buffer_index, BLOCK_SIZE);
    buffer_index += BLOCK_SIZE;
  }
}


int main()
{

  char* command_string = (char*)malloc(MAX_COMMAND_SIZE);
  FILE* fp = NULL;
  init();
  while (1)
  {
    // Print out the msh prompt
    printf("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(command_string, MAX_COMMAND_SIZE, stdin));

    /* Parse input */
    char* token[MAX_NUM_ARGUMENTS];

    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      token[i] = NULL;
    }

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char* argument_ptr = NULL;

    char* working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char* head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while (((argument_ptr = strsep(&working_string, WHITESPACE)) != NULL) &&
      (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this for loop and replace with your filesystem functionality

    // int token_index  = 0;
    // for( token_index = 0; token_index < token_count; token_index ++ ) 
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index] );  
    // }

    if (token[0] == NULL)
    {
      continue;
    }
    else if (strcmp(token[0], "quit") == 0)
    {
      exit(0);
    }
    else if (strcmp("savefs", token[0]) == 0)
    {
      savefs();
    }
    else if (strcmp("open", token[0]) == 0)
    {
      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }
      openfs(token[1]);;
    }
    else if (strcmp("close", token[0]) == 0)
    {
      closefs();;
    }
    else if (strcmp("list", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened\n");
        continue;
      }
      int h = 0;
      int a = 0;
      
      // list [-h] [-a]   Checking if -h is given. if -a is also given, then 
      // printing the attributes as well
      if (token[1] != NULL)
      {
        if (strcmp("-h", token[1]) == 0)
        {
          h = 1;
        }

        if (token[2] != NULL)
        {
          if (strcmp("-a", token[2]) == 0)
          {
            a = 1;
          }
        }
      }

      list(h, a);
    }


    // createfs command - Creates a new filesystem image
    /*shall create a file system image file with the named provided by the user.
    If the file name is not provided a message shall be printed: ERROR:
    createfs: Filename not provided*/

    else if (strcmp(token[0], "createfs") == 0)
    {
      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
      }
      else
      {
        createfs(token[1]);
      }
    }
    else if (strcmp(token[0], "df") == 0)
    {
      df();
    }

    else if (strcmp(token[0], "open") == 0)
    {
      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
      }
      else
      {
        openfs(token[1]);
      }
    }

    // reading the number of bytes
    // read <filename> <starting byte> <number of bytes>
    // Print <number of bytes> bytes from the file, in hexadecimal, starting at <starting byte>
    else if (strcmp("read", token[0]) == 0)
    {
      if (token_count < 4)
      {
        printf("ERROR: Invalid command syntax.\n");
        continue;
      }

      char* filename = token[1];
      int starting_byte = atoi(token[2]);
      int num_bytes = atoi(token[3]);

      readfile(filename, starting_byte, num_bytes);
    }

    // XOR is symmetric, so encrypting and decrypting are the same operation
    else if (strcmp(token[0], "encrypt") == 0 || strcmp(token[0], "decrypt") == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened\n");
        continue;
      }

      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
      }
      else if (token[2] == NULL)
      {
        printf("ERROR: No cipher specified\n");
      }
      else
      {
        uint8_t cipher_converted = (uint8_t)strtol(token[2], NULL, 16);
        encrypt(token[1], cipher_converted);

      }
    }
    else
    {
      printf("ERROR: Command not found\n");
    }


    // Cleanup allocated memory
    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      if (token[i] != NULL)
      {
        free(token[i]);
      }
    }

    free(head_ptr);

  }

  free(command_string);

  return 0;


}