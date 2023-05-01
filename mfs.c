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
#include <sys/stat.h>
#include <time.h>
#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define WHITESPACE " \t\n"   // We want to split our command line up into tokens
#define MAX_COMMAND_SIZE 255 // The maximum command-line size
#define MAX_NUM_ARGUMENTS 5
#define FIRST_DATA_BLOCK 790

#define HIDDEN 0x1
#define READ_ONLY 0x2

#define MAX_FILENAME_SIZE 64

uint8_t data[NUM_BLOCKS][BLOCK_SIZE];

uint8_t *free_blocks;
uint8_t *free_inodes;

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
  int size;
  char timestamp[32];
};

struct directoryEntry *directory;
struct inode *inodes;
FILE *fp;
char image_name[64];
uint8_t image_open;
int status;      // Hold the status of all return values.
struct stat buf; // stat struct to hold the returns from the stat call

/*
Name: init
Params: none
Returns: none
Description: Initializer function to set all the data structures to null, zero, or -1.
              Also assigns the pointers to the appropriate locations in the data array.

*/

void init()
{
  directory = (struct directoryEntry *)&data[0][0];
  inodes = (struct inode *)&data[20][0];

  free_blocks = (uint8_t *)&data[277][0];
  free_inodes = (uint8_t *)&data[19][0];

  memset(image_name, 0, 64);
  image_open = 0;

  int i, j;
  for (i = 0; i < NUM_FILES; i++)
  {
    directory[i].in_use = 0;
    directory[i].inode = -1;
    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);

    for (j = 0; j < BLOCKS_PER_FILE; j++)
    {
      inodes[i].blocks[j] = -1;
      inodes[i].in_use = 0;
      inodes[i].attribute = 0;
      inodes[i].size = 0;
      memset(inodes[i].timestamp, 0, 32);
    }
  }

  for (j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      int inode_idx = directory[i].inode;
      free_inodes[inode_idx] = 0;
      for (j = 0; j < BLOCKS_PER_FILE; j++)
      {
        int block_idx = inodes[inode_idx].blocks[j];
        if (block_idx != -1)
        {
          free_blocks[block_idx] = 0;
        }
      }
    }
  }
}

/*
Name: find_free_block
Params: none
Returns: int - index of the first free block
Description: Finds the first free block in the data array starting from the first data block
              and returns its index. If no free block is found, returns -1.
*/

int find_free_block()
{
  for (int i = FIRST_DATA_BLOCK; i < NUM_BLOCKS; i++)
  {
    if (!free_blocks[i])
    {
      printf("Found free block: %d\n", i);
      return i;
    }
  }
  return -1;
}

/*
Name : insert
Params: const char *filename - name of the file to be inserted
Returns: none
Description: Inserts the file with filename into the file system image. checks if there is 
              enough space and free directory and inode entry to store the file. If there is then
              it copies the file into the data array and updates the directory and inode entries.
*/

void insert(const char *filename)
{
  if (!image_open)
  {
    printf("ERROR: Disk image is not opened\n");
    return;
  }

  if (filename == NULL)
  {
    printf("ERROR: No filename specified\n");
    return;
  }

  int free_inode_idx = -1;
  int free_directory_entry_idx = -1;

  for (int i = 0; i < NUM_FILES; i++)
  {

    if (!directory[i].in_use)
    {
      free_directory_entry_idx = i;
    }

    if (!inodes[i].in_use)
    {
      free_inode_idx = i;
    }

    if (free_directory_entry_idx != -1 && free_inode_idx != -1)
    {
      break;
    }
  }

  FILE *input_file = fopen(filename, "rb");
  if (!input_file)
  {
    printf("ERROR: Unable to open file: %s\n", filename);
    return;
  }

  fseek(input_file, 0, SEEK_END);
  long file_size = ftell(input_file);
  fseek(input_file, 0, SEEK_SET);

  printf("File size: %ld\n", file_size);

  int required_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (required_blocks > BLOCKS_PER_FILE)
  {
    printf("ERROR: Not enough disk space 1.\n");
    fclose(input_file);
    return;
  }

  directory[free_directory_entry_idx].in_use = 1;
  strncpy(directory[free_directory_entry_idx].filename, filename, MAX_FILENAME_SIZE);
  directory[free_directory_entry_idx].inode = free_inode_idx;

  inodes[free_inode_idx].in_use = 1;
  inodes[free_inode_idx].attribute = 0;
  inodes[free_inode_idx].size = file_size;
  time_t raw_time = time(NULL);
  struct tm *formatted_time = localtime(&raw_time);
  strftime(inodes[free_inode_idx].timestamp, 32, "%H:%M:%S", formatted_time);

  printf("Required blocks: %d\n", required_blocks);
  for (int i = 0; i < required_blocks; i++)
  {
    int free_block_idx = find_free_block();
    if (free_block_idx == -1)
    {
      printf("ERROR: Not enough disk space 2.\n");
      fclose(input_file);
      return;
    }

    free_blocks[free_block_idx] = 1;
    inodes[free_inode_idx].blocks[i] = free_block_idx;
    fread(data[free_block_idx], sizeof(uint8_t), BLOCK_SIZE, input_file);
  }

  fclose(input_file);
  printf("File '%s' inserted successfully.\n", filename);
}

/*
Name : retrieve
Params: char *filename - name of the file to be retrieved
        char *newfilename - name of the file to be saved as
Returns: none
Description: Retrieves the file with filename from the file system image. checks if the file exists
              and if it does then it copies the file from the data array and saves it as newfilename.
*/

void retrieve(char *filename, char *newfilename)
{
  if (!image_open)
  {
    printf("ERROR: Disk image is not opened\n");
    return;
  }

  if (filename == NULL)
  {
    printf("ERROR: No filename specified\n");
    return;
  }

  int inode_idx = -1;
  for (int i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      inode_idx = directory[i].inode;
      break;
    }
  }

  if (inode_idx == -1)
  {
    printf("ERROR: File not found\n");
    return;
  }

  if (newfilename != NULL)
  {
    filename = newfilename;
  }

  struct inode *inode = &inodes[inode_idx];

  int file_size = inode->size;

  int remaining_bytes = file_size;
  int offset = 0;

  FILE *output_file = fopen(filename, "w");
  if (!output_file)
  {
    printf("Error: File not found.\n");
    return;
  }

  printf("Writing %d bytes to %s\n", file_size, filename);

  for (int i = 0; i < BLOCKS_PER_FILE && remaining_bytes > 0; i++)
  {
    int block_idx = inode->blocks[i];
    if (block_idx == -1)
    {
      continue;
    }

    int num_bytes = remaining_bytes < BLOCK_SIZE ? remaining_bytes : BLOCK_SIZE;

    fwrite(data[block_idx], 1, num_bytes, output_file);

    remaining_bytes -= num_bytes;
    offset += num_bytes;
  }

  fclose(output_file);
}

/*
Name : df
Params: none
Returns: none
Description: Prints the number of free bytes in the file system image in bytes.
*/

void df()
{
  int j;
  int count = 0;

  for (j = FIRST_DATA_BLOCK; j < NUM_BLOCKS; j++)
  {
    if (!free_blocks[j])
    {
      count++;
    }
  }
  printf("%d bytes free\n", count * BLOCK_SIZE);
}

/*
Name : createfs
Params: char *filename - name of the file system image to be created in memory
Returns: none
Description: Creates a file system image in memory with the name filename.
              Also initializes the data array to all 0s and sets the image_open flag to 1.
*/

void createfs(char *filename)
{
  fp = fopen(filename, "w+");

  strncpy(image_name, filename, strlen(filename));

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);
  image_open = 1;
}

/*
Name : list
Params: int h - flag to indicate if hidden files should be listed
        int a - flag to indicate if attributes should be listed
Returns: none
Description: Lists all the files in the file system image with their sizes and timestamp of creation.
              If h is set then hidden files are also listed. If a is set then attributes are also listed.
*/
void list(int h, int a)
{
  int i;
  int not_found = 1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {

      if (h && inodes[directory[i].inode].attribute & 1)
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
        printf("\t");
        printf("%d", inodes[directory[i].inode].size);
        printf("\t");
        printf("%s", inodes[directory[i].inode].timestamp);
        printf("\n");
      }
      else if (!h && !(inodes[directory[i].inode].attribute & 1))
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

        printf("\t");
        printf("%d", inodes[directory[i].inode].size);
        printf("\t");
        printf("%s", inodes[directory[i].inode].timestamp);
        printf("\n");
      }
    }
  }

  if (not_found)
  {
    printf("list: No files found.\n");
  }
}

/*
Name : savefs
Params: none
Returns: none
Description: Saves the current state of the file system image to the disk image from memory.
*/
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
  fp = NULL;
}

/*
Name : openfs
Params: filename - name of the file system image to open
Returns: none
Description: Opens the file system image specified by filename and loads it into memory.
*/
void openfs(char *filename)
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
}

/*
Name : readfile
Params: filename - name of the file to read
        starting_byte - the byte offset to start reading from
        num_bytes - the number of bytes to read
Returns: none
Description: Displays the specfied file starting at starting_byte and reading num_bytes to the screen.
              similar to "cat" but here the num of bytes to read is needed
*/
void readfile(char *filename, int starting_byte, int num_bytes)
{
  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      struct inode *inode_ptr = &inodes[directory[i].inode];

      int byte_offset = starting_byte;
      int bytes_left = num_bytes;

      for (int j = 0; j < BLOCKS_PER_FILE; j++)
      {
        if (inode_ptr->blocks[j] == -1)
        {
          break;
        }

        int block_offset = 0;

        if (byte_offset > 0)
        {
          block_offset = byte_offset % BLOCK_SIZE;
        }

        int bytes_to_read = BLOCK_SIZE - block_offset;
        if (bytes_to_read > bytes_left)
        {
          bytes_to_read = bytes_left;
        }

        if (bytes_to_read <= 0)
        {
          break;
        }

        for (int k = block_offset; k < block_offset + bytes_to_read; k++)
        {
          printf("%02X ", data[inode_ptr->blocks[j]][k]);
        }

        printf("\n");

        byte_offset += bytes_to_read;
        bytes_left -= bytes_to_read;
      }

      return;
    }
  }

  printf("ERROR: File not found.\n");
}

/*
Name : closefs
Params: none
Returns: none
Description: Closes the file system image from memory and writes it to computer's disk.
*/

void closefs()
{
  if (image_open == 0)
  {
    printf("ERROR: Disk image is not open.\n");
    return;
  }

  if (fp == NULL)
  {
    printf("ERROR: File already closed by savefs.\n");
    return;
  }
  fclose(fp);
  image_open = 0;
  memset(image_name, 0, 64);
}

/*
Name : delete
Params: filename - name of the file to delete
Returns: none
Description: Deletes the specified file from the file system image from memory.
*/
void delete (char *filename)
{
  int i;
  int file_found = 0;
  int32_t file_inode = -1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      if (strcmp(directory[i].filename, filename) == 0)
      {
        file_found = 1;
        file_inode = directory[i].inode;
        break;
      }
    }
  }
  if (!file_found)
  {
    printf("ERROR: File not found.\n");
    return;
  }

  if (inodes[file_inode].attribute & READ_ONLY)
  {
    printf("ERROR: File is read only.\n");
    return;
  }

  directory[i].in_use = 0;

  inodes[file_inode].in_use = 0;

  int j;
  for (j = 0; j < BLOCKS_PER_FILE; j++)
  {
    if (inodes[file_inode].blocks[j] != -1)
    {
      free_blocks[inodes[file_inode].blocks[j]] = 1;
      // inodes[file_inode].blocks[j] = -1;
    }
  }
}

/*
Name : undelete
Params: filename - name of the file to undelete
Returns: none
Description: Undoes the effect of delete by restoring the specified file to the file system image from memory.
              Does not work if data block has been overwritten.
*/
void undelete(char *filename)
{
  int i;
  int file_found = 0;
  int32_t file_inode = -1;

  // Find the deleted file in the directory
  for (i = 0; i < NUM_FILES; i++)
  {
    if (strcmp(directory[i].filename, filename) == 0)
    {
      printf("%d \n", directory[i].inode);
      file_found = 1;
      file_inode = directory[i].inode;
      break;
    }
  }

  if (!file_found)
  {
    printf("ERROR: File not found.\n");
    return;
  }

  if (inodes[file_inode].in_use)
  {
    printf("ERROR: File is not deleted.\n");
    return;
  }

  directory[i].in_use = 1;
  inodes[file_inode].in_use = 1;

  for (int j = 0; j < BLOCKS_PER_FILE; j++)
  {
    if (inodes[file_inode].blocks[j] != -1)
    {
      free_blocks[inodes[file_inode].blocks[j]] = 0;
    }
  }
}

/*
Name : encrypt_
Params: filename - name of the file to encrypt or decrypt on the file system image in memory
        cipher - cipher to use for encryption, idealy should be uint8_t but if larger, it wraps around
Returns: none
Description: Encrypts the specified file in the file system image in memory using the specified cipher using 
              the byte to byte XOR cipher.
              Same function is called to decrypt the file
*/

void encrypt_(char *filename, uint8_t cipher)
{
  if (!image_open)
  {
    printf("ERROR: Disk image is not opened\n");
    return;
  }

  if (filename == NULL)
  {
    printf("ERROR: No filename specified\n");
    return;
  }

  int inode_idx = -1;
  for (int i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      inode_idx = directory[i].inode;
      break;
    }
  }

  if (inode_idx == -1)
  {
    printf("ERROR: File not found\n");
    return;
  }

  struct inode *inode = &inodes[inode_idx];
  int file_size = inode->size;
  int remaining_bytes = file_size;

  for (int i = 0; i < BLOCKS_PER_FILE && remaining_bytes > 0; i++)
  {
    int block_idx = inode->blocks[i];
    if (block_idx == -1)
    {
      continue;
    }

    int num_bytes = remaining_bytes < BLOCK_SIZE ? remaining_bytes : BLOCK_SIZE;

    for (int j = 0; j < num_bytes; j++)
    {
      data[block_idx][j] ^= cipher;
    }

    remaining_bytes -= num_bytes;
  }
}

/*
Name : attrib
Params: attrib_str - string containing the attributes to set
        filename - name of the file to set the attributes for
Returns: none
Description: Sets the attributes of the specified file in the file system image in memory. the attributes are set as 
              +h - hidden
              -h - not hidden
              +r - read only
              -r - undo read only
*/

void attrib(char *attrib_str, char *filename)
{
  int i;
  int file_found = 0;
  int32_t file_inode = -1;

  file_inode = -1;

  for (i = 0; i < NUM_FILES; i++)
  {
    if (strncmp(directory[i].filename, filename, strlen(filename)) == 0)
    {
      file_found = 1;
      file_inode = directory[i].inode;
      break;
    }
  }

  if (!file_found)
  {
    printf("attrib: File not found.\n");
    return;
  }

  int attrib_value = inodes[file_inode].attribute;

  int j;
  for (j = 0; j < strlen(attrib_str); j++)
  {
    switch (attrib_str[j])
    {
    case '+':
      j++;
      switch (attrib_str[j])
      {
      case 'h':
        attrib_value |= HIDDEN;
        break;
      case 'r':
        attrib_value |= READ_ONLY;
        break;
      default:
        printf("ERROR: Invalid attribute.\n");
        return;
      }
      break;
    case '-':
      j++;
      switch (attrib_str[j])
      {
      case 'h':
        attrib_value &= ~HIDDEN;
        break;
      case 'r':
        attrib_value &= ~READ_ONLY;
        break;
      default:
        printf("ERROR: Invalid attribute.\n");
        return;
      }
      break;
    default:
      printf("ERROR: Invalid attribute.\n");
      return;
    }
  }

  inodes[file_inode].attribute = attrib_value;
}

/*
Name : main 
Params: none
Returns: int - status code
Description: Main function of the program. Runs a continuous loop for the prompt, tokenizes it and upon comparing
              the tokens that what is required, calls the appropriate function using the strcmp function

*/
int main()
{

  char *command_string = (char *)malloc(MAX_COMMAND_SIZE);
  //FILE* fp = NULL;
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
    while (!fgets(command_string, MAX_COMMAND_SIZE, stdin))
      ;

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      token[i] = NULL;
    }

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;

    char *working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

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
      openfs(token[1]);
      ;
    }
    else if (strcmp("close", token[0]) == 0)
    {
      closefs();
      ;
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

      if (token[1] != NULL)
      {
        if (strcmp("-h", token[1]) == 0)
        {
          h = 1;
        }
        else if (strcmp("-a", token[1]) == 0)
        {
          a = 1;
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
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }

      df();
    }

    else if (strcmp(token[0], "delete") == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }

      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified.\n");
      }

      delete (token[1]);
    }

    else if (strcmp(token[0], "undelete") == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }

      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified.\n");
      }

      undelete(token[1]);
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

    else if (strcmp("read", token[0]) == 0)
    {
      if (token_count < 4)
      {
        printf("ERROR: Invalid command syntax.\n");
        continue;
      }

      char *filename = token[1];
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
        uint8_t cipher_converted = (uint8_t)atoi(token[2]);
        encrypt_(token[1], cipher_converted);
      }
    }

    else if (strcmp("attrib", token[0]) == 0)
    {
      if (token[1] == NULL)
      {
        printf("ERROR: No attribute specified\n");
        continue;
      }

      if (token[2] == NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }
      attrib(token[1], token[2]);
    }

    else if (strcmp(token[0], "insert") == 0)
    {
      insert(token[1]);
    }

    else if (strcmp(token[0], "retrieve") == 0)
    {
      if (token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }

      char *fn = token[1];
      char *fn2 = NULL;

      if (token[2] != NULL)
      {
        fn2 = token[2];
      }

      retrieve(fn, fn2);
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