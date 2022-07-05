#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "minitar.h"

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 512

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *)header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%7o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or 1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return 1;
    }

    strncpy(header->name, file_name, 100); // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%7o", stat_buf.st_mode & 07777); // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%7o", stat_buf.st_uid); // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid); // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return 1;
    }
    strncpy(header->uname, pwd->pw_name, 32); // Owner  name of the file, null-terminated string

    snprintf(header->gid, 8, "%7o", stat_buf.st_gid); // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid); // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return 1;
    }
    strncpy(header->gname, grp->gr_name, 32); // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%11o", (unsigned)stat_buf.st_size); // File size, 0-padded octal
    snprintf(header->mtime, 12, "%11o", (unsigned)stat_buf.st_mtime); // Modification time, 0-padded octal
    header->typeflag = REGTYPE; // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6); // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2); // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%7o", major(stat_buf.st_dev)); // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%7o", minor(stat_buf.st_dev)); // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
* Removes 'nbytes' bytes from the file identified by 'file_name'
* Returns 0 upon success, -1 upon error
* Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
*/
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];
    // Note: ftruncate does not work with O_APPEND
    int fd = open(file_name, O_WRONLY);
    if (fd == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to open file %s", file_name);
        perror(err_msg);
        return 1;
    }
    //  Seek to end of file - nbytes
    off_t current_pos = lseek(fd, -1 * nbytes, SEEK_END);
    if (current_pos == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to seek in file %s", file_name);
        perror(err_msg);
        close(fd);
        return 1;
    }
    // Remove all contents of file past current position
    if (ftruncate(fd, current_pos) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}


int create_archive(const char *archive_name, const file_list_t *files) {

    // needed variables:
    FILE *archive = fopen(archive_name,"w");   // final archive file
    node_t *current_node = files->head;         // iterate through files in linked list
    FILE *current_file;                         // used as pointer to each file
    struct stat st;                             // used to grab file stats 
    void *block = malloc(512);                  // used to write to archive file in blocks
    long bytes_written = 0;                     // used for testing purposes

    for(int i = 0; i < files->size; i++){   // for each file;

        // check if file is valid:
        if(stat(current_node->name,&st)==-1){ printf("%s is not a valid file\n",current_node->name);}

        
        else{    
            
            // file is valid: open file
            current_file = fopen(current_node->name,"rb");

            // write header to archive
            fill_tar_header(block,current_node->name);
            fwrite(block,512,1,archive);
            bytes_written += 512;

            // get number of bytes and blocks
            long num_bytes = st.st_size;
            int num_blocks = num_bytes / 512;
            int remainder = num_bytes % 512;
            
            // testing purposes
            // printf("File size: %d blocks plus %d bytes: %ld total bytes\n",num_blocks,remainder,num_bytes);

            // write each block to the archive file
            for(int i = 0; i < num_blocks;i++){
                memset(block, 0, 512);
                fread(block,512,1,current_file);
                fwrite(block,512,1,archive);
                bytes_written += 512;
            }

            // wrtie the final, oblong block to the archive file
            if(remainder){
                memset(block, 0, 512);
                fread(block,remainder,1,current_file);
                fwrite(block,remainder,1,archive);
                memset(block, 0, 512);
                fwrite(block,512-remainder,1,archive);
                bytes_written += 512;
            }
            fclose(current_file);
        } 

        current_node = current_node->next;
    }

    // write footer to archive
    memset(block, 0, 512);
    fwrite(block,512,1,archive);
    fwrite(block,512,1,archive);
    bytes_written += 512;
    bytes_written += 512;

    // testing purposes
    // printf("%ld bytes writen to the tar file\n",bytes_written);

    // free all resources
    free(block);
    fclose(archive);
    return 0;
}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {

    // needed variables:
    FILE *archive = fopen(archive_name,"r+");   // open archive file for reading and writing
    fseek(archive,-1024,SEEK_END);              // overwrite footer
    node_t *current_node = files->head;         // iterate through files in linked list
    FILE *current_file;                         // used as pointer to each file
    struct stat st;                             // used to grab file stats 
    void *block = malloc(512);                  // used to store info that will be read/written
    long bytes_written = 0;                     // used for testing purposes

    for(int i = 0; i < files->size; i++){   // for each file;

        // check if file is valid:
        if(stat(current_node->name,&st)==-1){ printf("%s is not a valid file\n",current_node->name);}

        
        else{    
            
            // file is valid: open file
            current_file = fopen(current_node->name,"rb");

            // write header to archive
            fill_tar_header(block,current_node->name);
            fwrite(block,512,1,archive);
            bytes_written += 512;

            // get number of bytes and blocks
            long num_bytes = st.st_size;
            int num_blocks = num_bytes / 512;
            int remainder = num_bytes % 512;
            
            // testing purposes
            // printf("File size: %d blocks plus %d bytes: %ld total bytes\n",num_blocks,remainder,num_bytes);

            // write each block to the archive file
            for(int i = 0; i < num_blocks;i++){
                memset(block, 0, 512);
                fread(block,512,1,current_file);
                fwrite(block,512,1,archive);
                bytes_written += 512;
            }

            // wrtie the final, oblong block to the archive file
            if(remainder){
                memset(block, 0, 512);
                fread(block,remainder,1,current_file);
                fwrite(block,remainder,1,archive);
                memset(block, 0, 512);
                fwrite(block,512-remainder,1,archive);
                bytes_written += 512;
            }
            fclose(current_file);
        } 
        current_node = current_node->next;
    }

    // write footer to archive
    memset(block, 0, 512);
    fwrite(block,512,1,archive);
    fwrite(block,512,1,archive);
    bytes_written += 512;
    bytes_written += 512;

    // testing purposes
    // printf("%ld bytes writen to the tar file\n",bytes_written);

    // free all resources
    free(block);
    fclose(archive);
    return 0;
}

int get_archive_file_list(const char *archive_name, file_list_t *files) {

    // needed variables:
    FILE *archive = fopen(archive_name,"r");    // archive file
    char *block = malloc(100 * sizeof(char));   // space to read data to

    // get first name from header
    fread(block,100,1,archive);

    // while name is valid (not the footer)
    while((int) block[0] != 0){

        // append name to the list
        file_list_add(files,block);

        // jump to "size" field in the header
        fseek(archive,24,SEEK_CUR);

        // grab size from header (size is stored as an octal)
        memset(block,0,100);
        fread(block,12,1,archive);

        // convert from octal to decimal. store in "num_bytes"
        long num_bytes = 0;
        int multiplier = 1;
        for(int i = (strlen(block) - 1); i >= 0;i--){ // iterate from least to most significant digit
            // translate char to int
            int n = (int)block[i] - 48;
            // if number is not valid, stop
            if(n == -16){break;}
            // update variables
            num_bytes += n * multiplier;
            multiplier *= 8;
        }

        // skip the rest of the header
        fseek(archive,376,SEEK_CUR);

        // jump over the rest of the blocks in the file
        int num_blocks = num_bytes / 512;
        if(num_bytes % 512){num_blocks++;}
        fseek(archive,(512 * num_blocks),SEEK_CUR);

        // read the next name
        fread(block,100,1,archive);        
    }
    
    // free resources 
    free(block);
    fclose(archive);
    return 0;
}

int extract_files_from_archive(const char *archive_name) {

    // needed variables:
    FILE *archive = fopen(archive_name,"r");    // open archive file for reading and writing
    char *block = malloc(512);                  // space to read/write data to name
    fread(block,100,1,archive);                 // read first file's name

    // while file name is valid (aka, not the footer)
    while((int) block[0] != 0){

        // open new file with the currently saved name
        FILE *current_file = fopen(block,"w"); 

        // jump to "size" field in the header
        fseek(archive,24,SEEK_CUR);

        // copy size (stored as an octal-string)
        memset(block,0,512);
        fread(block,12,1,archive);

        // convert from octal to decimal, store in "num_bytes"
        long num_bytes = 0;
        int multiplier = 1;
        for(int i = (strlen(block) - 1); i >= 0;i--){
            int n = (int)block[i] - 48;
            if(n == -16){break;}
            num_bytes += n * multiplier;
            multiplier *= 8;
        }

        // jump over the rest of the header
        fseek(archive,376,SEEK_CUR);

        // write each block of data to the new file
        int num_blocks = num_bytes / 512;
        for(int i = 0; i < num_blocks; i++){
            fread(block,512,1,archive);
            fwrite(block,512,1,current_file);
        }

        // write the final, oblong block of data
        int remainder = num_bytes % 512;
        if(remainder){
            fread(block,remainder,1,archive);
            fwrite(block,remainder,1,current_file);
            fseek(archive,(512 - remainder),SEEK_CUR);
            memset(block,0,512);
        }

        fclose(current_file);

        // read the next file's name
        fread(block,100,1,archive);
        
    }
    
    free(block);
    return 0;
}