#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <pthread.h>

#define S 30
#define MAX_SIZE_PIPENAME 40

/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/* operation codes (for client-server requests) */
enum {
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

typedef struct __attribute__((__packed__)) s_mount{

    char pipename[MAX_SIZE_PIPENAME];

}mount_message;

typedef struct  __attribute__((__packed__)) s_unmount{

    int session_id;

}unmount_message;

typedef struct  __attribute__((__packed__)) s_open{

    int session_id;
    char name[MAX_SIZE_PIPENAME];
    int flags;
    
}open_message;

typedef struct  __attribute__((__packed__)) s_close{

    int session_id;
    int fhandle;
    
}close_message;

typedef struct  __attribute__((__packed__)) s_write_read{

    int session_id;
    int fhandle;
    size_t len;

}write_read_message;

typedef struct  __attribute__((__packed__)) s_shutdown{

    int session_id;

}shutdown_message;

typedef struct client {

    char client_pipename[MAX_SIZE_PIPENAME];
    int fd_client_pipe;
    int session_id;

} Client;

typedef struct working_thread{

    void *buffer;
    pthread_t tid;
    pthread_cond_t cond;
    pthread_mutex_t mutex_lock;
    int id;

} Working_thread;

#endif /* COMMON_H */