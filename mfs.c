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
};

struct directoryEntry* directory;
struct inode* inodes;
FILE* fp;
char image_name[64];
uint8_t image_open;
int    status;                   // Hold the status of all return values.
struct stat buf;                 // stat struct to hold the returns from the stat call
//add all the functions here

void init()
{
  directory = (struct directoryEntry*)&data[0][0];
  inodes = (struct inode*)&data[20][0];

  free_blocks = (uint8_t *)&data[277][0];
  free_inodes = (uint8_t *)&data[19][0];

  memset(image_name, 0, 64);
  image_open = 0;

  int i, j, k;
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
    }
  }

  for (j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }

  // Mark used inodes and blocks as not free
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use) {
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

  // directory[0].in_use = 1;
  // strncpy(directory[0].filename, "file.txt", strlen("file.txt"));
}


//find free block
int find_free_block()
{
  for (int i = FIRST_DATA_BLOCK; i < NUM_BLOCKS; i++)
  {
    if (free_blocks[i])
    {
      return i;
    }
  }
  return -1;
}


void insert(const char* filename)
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

  if (strlen(filename) > MAX_FILENAME_SIZE)
  {
    printf("ERROR: Filename too long\n");
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

  if (free_directory_entry_idx == -1 || free_inode_idx == -1)
  {
    printf("ERROR: Not enough disk space.\n");
    return;
  }

  FILE* input_file = fopen(filename, "rb");
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
    printf("ERROR: Not enough disk space.\n");
    fclose(input_file);
    return;
  }

  directory[free_directory_entry_idx].in_use = 1;
  strncpy(directory[free_directory_entry_idx].filename, filename, MAX_FILENAME_SIZE);
  directory[free_directory_entry_idx].inode = free_inode_idx;

  inodes[free_inode_idx].in_use = 1;
  inodes[free_inode_idx].attribute = 0;

  printf("Required blocks: %d\n", required_blocks);
  for (int i = 0; i < required_blocks; i++)
  {
    int free_block_idx = find_free_block();
    if (free_block_idx == -1)
    {
      printf("ERROR: Not enough disk space.\n");
      fclose(input_file);
      return;
    }

    free_blocks[free_block_idx] = 0;
    inodes[free_inode_idx].blocks[i] = free_block_idx;
    fread(data[free_block_idx], sizeof(uint8_t), BLOCK_SIZE, input_file);
  }

  fclose(input_file);
  printf("File '%s' inserted successfully.\n", filename);
}

void retrieve(char *filename)
{
  fp = fopen(filename, "r");  
  if( fp == NULL )
  {
    printf("Could not open output file: %s\n", filename );
    perror("Opening output file returned");    
  }
  status =  stat( filename, &buf ); 
  if(status != -1)
  {
    // Initialize our offsets and pointers just we did above when reading from the file.
    int block_index = 0;
    int copy_size   = buf . st_size;
    int offset      = 0;
    for (int i = 0; i < NUM_FILES; i++)
    {
      if(directory[i].in_use==1){        
        if(strcmp(directory[i].filename,filename)==0){
          int tmp_idx = directory[i].inode;
          if(inodes[tmp_idx].in_use==1)
          {
            block_index = tmp_idx;
          }                    
        }
      }      
    }    
    // struct inode* get_inode = &inodes[block_index];
    // get_inode->in_use = 1;
    filename = basename(filename);
    printf("Writing %d bytes to %s\n", (int) buf . st_size, filename );

    // Using copy_size as a count to determine when we've copied enough bytes to the output file.
    // Each time through the loop, except the last time, we will copy BLOCK_SIZE number of bytes from
    // our stored data to the file fp, then we will increment the offset into the file we are writing to.
    // On the last iteration of the loop, instead of copying BLOCK_SIZE number of bytes we just copy
    // how ever much is remaining ( copy_size % BLOCK_SIZE ).  If we just copied BLOCK_SIZE on the
    // last iteration we'd end up with gibberish at the end of our file. 
    while( copy_size > 0 )
    { 

      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else 
      {
        num_bytes = BLOCK_SIZE;
      }
      fp = fopen(filename,"w");
      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( data[block_index], 1, num_bytes, fp);

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( fp, offset, SEEK_SET );      
    }

    // Close the output file, we're done. 
    fclose( fp );
  }
  else
  {
    printf("Unable to open file: %s\n", filename );
    perror("Opening the input file returned: ");    
  }
}

void retrieve1(char *filename,char *newfilename)
{
  fp = fopen(filename, "r");  
  if( fp == NULL )
  {
    printf("Could not open output file: %s\n", filename );
    perror("Opening output file returned");    
  }
  status =  stat( filename, &buf ); 
  if(status != -1)
  {
    // Initialize our offsets and pointers just we did above when reading from the file.
    int block_index = 0;
    int copy_size   = buf . st_size;
    int offset      = 0;
    for (int i = 0; i < NUM_FILES; i++)
    {
      if(directory[i].in_use==1){        
        if(strcmp(directory[i].filename,filename)==0){
          int tmp_idx = directory[i].inode;
          if(inodes[tmp_idx].in_use==1)
          {
            block_index = tmp_idx;
          }                    
        }
      }      
    }    
    // struct inode* get_inode = &inodes[block_index];
    // get_inode->in_use = 1;    
    printf("Writing %d bytes to %s\n", (int) buf . st_size, newfilename );

    // Using copy_size as a count to determine when we've copied enough bytes to the output file.
    // Each time through the loop, except the last time, we will copy BLOCK_SIZE number of bytes from
    // our stored data to the file fp, then we will increment the offset into the file we are writing to.
    // On the last iteration of the loop, instead of copying BLOCK_SIZE number of bytes we just copy
    // how ever much is remaining ( copy_size % BLOCK_SIZE ).  If we just copied BLOCK_SIZE on the
    // last iteration we'd end up with gibberish at the end of our file. 
    while( copy_size > 0 )
    { 

      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else 
      {
        num_bytes = BLOCK_SIZE;
      }
      fp = fopen(newfilename,"w");
      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( data[block_index], 1, num_bytes, fp);

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( fp, offset, SEEK_SET );      
    }

    // Close the output file, we're done. 
    fclose( fp );
  }
  else
  {
    printf("Unable to open file: %s\n", filename );
    perror("Opening the input file returned: ");    
  }
}

//creating function df
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

void createfs(char* filename)
{
  fp = fopen(filename, "w+");

  strncpy(image_name, filename, strlen(filename));

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);
  image_open = 1;

  fclose(fp);
}


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

        printf("\n");
      }
    }
  }

  if (not_found)
  {
    printf("list: No files found.\n");
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
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      struct inode* inode_ptr = &inodes[directory[i].inode];

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

//a function to delete a file from the file system
void delete(char* filename)
{
  int i;
  int file_found = 0;
  int32_t file_inode = -1;

  for(i =0; i < NUM_FILES; i++)
  {
    if(directory[i].in_use)
    {
      if(strcmp(directory[i].filename, filename) ==0)
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

  // #define READ_ONLY 0x2
  // check if it read only
  if (inodes[file_inode].attribute & READ_ONLY)
  {
    printf("ERROR: File is read only.\n");
    return;
  }

  directory[i].in_use = 0;

  inodes[file_inode].in_use =0;

  int j;
  for(j = 0; j < BLOCKS_PER_FILE; j++)
  {
    if(inodes[file_inode].blocks[j] != -1)
    {
      free_blocks[inodes[file_inode].blocks[j]] = 1;
      // inodes[file_inode].blocks[j] = -1;
    }
  }
}

//a function to undelete a file from the file system
void undelete(char* filename)
{
  int i;
  int file_found = 0;
  int32_t file_inode = -1;

  // Find the deleted file in the directory
  for(i =0; i < NUM_FILES; i++)
  {
    if(strcmp(directory[i].filename, filename) == 0)
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

  // Check if the file is already in use
  if (inodes[file_inode].in_use)
  {
    printf("ERROR: File is not deleted.\n");
    return;
  }

  directory[i].in_use = 1;
  inodes[file_inode].in_use = 1;

  // Mark all blocks used by the file as in use
  for (int j = 0; j < BLOCKS_PER_FILE; j++)
  {
    if (inodes[file_inode].blocks[j] != -1)
    {
      free_blocks[inodes[file_inode].blocks[j]] = 0;
    }
  }

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

void attrib(char* attrib_str, char* filename)
{
  printf("attrib: %s %s\n", attrib_str, filename);
  int i;
  int file_found = 0;
  int32_t file_inode = -1;

  // Initialize file_inode to -1
  file_inode = -1;

  // Search the directory for the specified file
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
int main()
{

  char* command_string = (char*)malloc(MAX_COMMAND_SIZE);
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
    char* token[MAX_NUM_ARGUMENTS];

    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      token[i] = NULL;
    }

    int token_count = 0;

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

      // list [-h] [-a]   Checking if -h is given. if -a is also given, then

      // printing the attributes as well
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
      if( !image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }

      df();
    }
    
    else if(strcmp(token[0], "delete") == 0)
    {
      if( !image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }

      if(token[1] == NULL)
      {
        printf("ERROR: No filename specified.\n");
      }

      delete(token[1]);
    }

    else if(strcmp(token[0], "undelete") == 0)
    {
      if( !image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }

      if(token[1] == NULL)
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

      printf("token[1] : %s\n", token[1]);
      printf("token[2] : %s\n", token[2]);
      attrib(token[1], token[2]);
    }


    else if (strcmp(token[0], "insert") == 0)
    {
      insert(token[1]);
    }

    else if (strcmp(token[0], "retrieve") == 0)
    {
      if(token[1]==NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }
      if(token[1]!=NULL && token[2]==NULL)
      {
        retrieve(token[1]);        
      }
      else if(token[1]!=NULL && token[2]!=NULL)
      {
        retrieve1(token[1],token[2]);
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