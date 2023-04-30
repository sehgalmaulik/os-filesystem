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


uint8_t data [NUM_BLOCKS][BLOCK_SIZE];

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

struct directoryEntry *directory;
struct inode *inodes;
FILE *fp;
char image_name[64];
uint8_t image_open;


//add all the functions here

void init()
{
    directory =  (struct directoryEntry*) &data[0][0];
    inodes = (struct inode*) &data[20][0];

    int i;

    for( i = 0; i < NUM_FILES; i++ )
    {
        directory[i].in_use = 0;
        directory[i].inode = -1;

        memset( directory[i].filename, 0, 64 );

        int j;
        for( j = 0; j < BLOCKS_PER_FILE; j++ )
        {
            inodes[i].blocks[j] = -1;
            inodes[i].in_use = 0;
            inodes[i].attribute = 0;

        }
    }
}

void createfs( char *filename )
{
    fp = fopen( filename, "w+" );
    
    memset( data, 0, NUM_BLOCKS * BLOCK_SIZE );

    fclose(fp);
  
}

// LIST
void list()
{
    int i;
    int not_found = 1;
    
    for (i = 0; i < NUM_FILES; i++)
    {
        if (directory[i].in_use)
        {   
            not_found = 0;
            char filename[65];
            memset(filename, 0, 65);
            strncpy(filename, directory[i].filename, strlen(directory[i].filename));
            printf("%s\n", filename);
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
    }

    fp = fopen(image_name, "w");
    fwrite(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
    memset(image_name, 0, 64);
    fclose(fp);
}





int main()
{

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
  FILE *fp = NULL;
  while( 1 )
  {
    // Print out the msh prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      token[i] = NULL;
    }

    int   token_count = 0;                                 

    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;                                         

    char *working_string  = strdup( command_string );                

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
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

    if (token[0]==NULL)
    {
        continue;
    }

    // processs the filesystem commads

    // quit command
    if( strcmp( token[0], "quit" ) == 0 )
    {
      exit(0);
    }

    if (strcmp("savefs", token[0]) == 0)
    {
        savefs();
    }
    
    if (strcmp("list", token[0]) == 0)
    {
        if (!image_open)
        {
            printf("ERROR: Disk image is not opened\n");
            continue;
        }
        list();
    }

    

    // createfs command - Creates a new filesystem image
    /*shall create a file system image file with the named provided by the user.
    If the file name is not provided a message shall be printed: ERROR:
    createfs: Filename not provided*/

    if( strcmp( token[0], "createfs" ) == 0 )
    {
      if( token[1] == NULL )
      {
        printf("ERROR: No filename specified\n");
      }
      else
      {
        createfs( token[1] );
      }
    }













    // Cleanup allocated memory
    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      if( token[i] != NULL )
      {
        free( token[i] );
      }
    }

    free( head_ptr );

  }

  free( command_string );

  return 0;


}