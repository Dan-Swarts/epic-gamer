#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("%s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);
    

    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
    // to execute archive operations

      if(strcmp(argv[1],"-c") == 0){
         // populate list of files with inputs;
         for(int i = 4; i < argc; i++){
            file_list_add(&files,argv[i]);
         }
         // xreate tar;
         create_archive(argv[3],&files);

         // clear list;
         file_list_clear(&files);
        
      }

      else if(strcmp(argv[1],"-a") == 0){
         // populate list of files with inputs;
         for(int i = 4; i < argc; i++){
            file_list_add(&files,argv[i]);
         }

        // append files to end of tar;
        append_files_to_archive(argv[3], &files);

         // clear list;
         file_list_clear(&files);

      }

      else if(strcmp(argv[1],"-t") == 0){
         // populate list of files with contents of tar;
         get_archive_file_list(argv[3],&files);

         // print contents of list;
         node_t *current_node = files.head;
         while(current_node->next != NULL){
            printf("%s\n",current_node->name);
            current_node = current_node->next;
         }
         printf("%s\n",current_node->name);

         // clear list;
         file_list_clear(&files);
      }

      else if(strcmp(argv[1],"-u") == 0){
         // populate list of files with contents of tar;
         get_archive_file_list(argv[3],&files);

         // match all update files to contents of tar;
         int n = 1;
         for(int i = 4; i < argc; i++){
            if(!file_list_contains(&files,argv[i])){
               n = 0;
            }
         }


         // if all files are valid, update;
         if(n){

            // clear list;
            file_list_clear(&files);

            // populate list of files with inputs;
            for(int i = 4; i < argc; i++){
               file_list_add(&files,argv[i]);
            }

            // append files to end of tar;
            append_files_to_archive(argv[3], &files);
         }

         // clear list;
         file_list_clear(&files);
      }

     else if(strcmp(argv[1],"-x") == 0){
        // extract files;
        extract_files_from_archive(argv[3]);
     }

     else{
         printf("unknown operation\n");
     }
    
    return 0;
}
