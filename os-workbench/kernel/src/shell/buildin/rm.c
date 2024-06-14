#include <devices.h>
#include <klib.h>
#include <vfs.h>
#include <yls.h>
#include <dir.h>

int mysh_rm(void *args[]){
    if(args[1]){
        for(int i=1;args[i];++i){
            vfs->unlink(args[i]);
            error_print(" cannot remove '%s': ",args[i]);
        }
        return 0;
    }else{
        fprintf(2,"Usage: rm [FILE]\n");
        return -1;
    }
}

