#include "operations.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static Client clients[S];
static char free_sessions[S];
int online_clients = 0;

static Working_thread threads[S];

char server_pipename[MAX_SIZE_PIPENAME];
int sv_pipe;

int read_function(void* message_received , size_t message_size) {
    size_t bytes_read = 0;
    ssize_t curr = 0;

    while(bytes_read < message_size){
        curr = read(sv_pipe, message_received + bytes_read, message_size - bytes_read);
        if(curr == -1)
            return -1;
        bytes_read += (size_t)curr;
    }
    return 0;
}

int write_function(int c_pipe, void* to_write, size_t to_write_size) {
    size_t bytes_written = 0;
    ssize_t curr = 0;

    while(bytes_written < to_write_size){
        curr = write(c_pipe, to_write + bytes_written, to_write_size - bytes_written);
        if(curr == -1)
                 return -1;
        bytes_written += (size_t)curr;
    }
    return 0;
}

int server_handle_mount_call() {
    mount_message message_received;
    int session_id = 0;

    if(read_function(&message_received, sizeof(mount_message)) == -1){
        return -1;
    }

    int error = -1;
    if(online_clients == S){
        int c_pipe = open(message_received.pipename, O_WRONLY);
        if(c_pipe == -1)
            return -1;
        if(write_function(c_pipe, &error, sizeof(int)) == -1)
            return -1;
        if(close(c_pipe) == -1){
            return -1;
        }
        return 0;
    } 

    for(int i = 0; i < S; i++){
        if(free_sessions[i] == FREE){
            free_sessions[i] = TAKEN;
            session_id = i;
            online_clients++; 
            break;
        }
    }
 
    char op_code = '0' + TFS_OP_CODE_MOUNT;
    threads[session_id].buffer = (void*)malloc(sizeof(int) + sizeof(char) + sizeof(mount_message));
    memcpy(threads[session_id].buffer, &op_code, sizeof(char));
    memcpy(threads[session_id].buffer + sizeof(char), &session_id, sizeof(int));
    memcpy(threads[session_id].buffer + sizeof(int) + sizeof(char), &message_received, sizeof(mount_message));
    pthread_cond_signal(&threads[session_id].cond);
    return 0;
}

int server_handle_unmount_call() {
    unmount_message message_received;

    if(read_function(&message_received, sizeof(unmount_message)) == -1)
        return -1;

    int client_id = message_received.session_id;

    char op_code = '0' + TFS_OP_CODE_UNMOUNT;

    threads[client_id].buffer = (void*)malloc(sizeof(char) + sizeof(unmount_message));
    memcpy(threads[client_id].buffer, &op_code, sizeof(char));
    memcpy(threads[client_id].buffer + sizeof(char), &message_received, sizeof(unmount_message));
    pthread_cond_signal(&threads[client_id].cond);

    return 0;
}

int server_handle_open_call() {
    open_message message_received;

    if(read_function(&message_received, sizeof(open_message)) == -1)
        return -1;

    char op_code = '0' + TFS_OP_CODE_OPEN;
    int client_id = message_received.session_id;

    threads[client_id].buffer = (void*)malloc(sizeof(char) + sizeof(open_message));
    memcpy(threads[client_id].buffer, &op_code, sizeof(char));
    memcpy(threads[client_id].buffer + sizeof(char), &message_received, sizeof(open_message));
    pthread_cond_signal(&threads[client_id].cond);
    return 0;
}

int server_handle_close_call(){
    close_message message_received;

    if(read_function(&message_received, sizeof(close_message)) == -1)
        return -1;

    char op_code = '0' + TFS_OP_CODE_CLOSE;
    int client_id = message_received.session_id;

    threads[client_id].buffer = (void*)malloc(sizeof(char) + sizeof(close_message));
    memcpy(threads[client_id].buffer, &op_code, sizeof(char));
    memcpy(threads[client_id].buffer + sizeof(char), &message_received, sizeof(close_message));
    pthread_cond_signal(&threads[client_id].cond);
    return 0;
}

int server_handle_write_call() {
    write_read_message message_received;

    if(read_function(&message_received, sizeof(write_read_message)) == -1)
        return -1;

    size_t len = message_received.len;
    void *buffer_write[len];
    if(read_function(buffer_write, len) == -1)
        return -1;

    char op_code = '0' + TFS_OP_CODE_WRITE;
    int client_id = message_received.session_id;

    threads[client_id].buffer = (void*)malloc(sizeof(char) + sizeof(write_read_message) + len);
    memcpy(threads[client_id].buffer, &op_code, sizeof(char));
    memcpy(threads[client_id].buffer + sizeof(char), &message_received, sizeof(write_read_message));
    memcpy(threads[client_id].buffer + sizeof(char) + sizeof(write_read_message), buffer_write, len);
    pthread_cond_signal(&threads[client_id].cond);
    return 0;
}

int server_handle_read_call() {
    write_read_message message_received;

    if(read_function(&message_received, sizeof(write_read_message)) == -1)
        return -1;

    char op_code = '0' + TFS_OP_CODE_READ;
    int client_id = message_received.session_id;

    threads[client_id].buffer = (void*)malloc(sizeof(char) + sizeof(write_read_message));
    memcpy(threads[client_id].buffer, &op_code, sizeof(char));
    memcpy(threads[client_id].buffer + sizeof(char), &message_received, sizeof(write_read_message));
    pthread_cond_signal(&threads[client_id].cond);
    return 0;
}

int server_handle_shutdown_call() {
    shutdown_message message_received;

    if(read_function(&message_received, sizeof(shutdown_message)) == -1)
        return -1;

    char op_code = '0' + TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    int client_id = message_received.session_id;

    threads[client_id].buffer = (void*)malloc(sizeof(char) + sizeof(shutdown_message));
    memcpy(threads[client_id].buffer, &op_code, sizeof(char));
    memcpy(threads[client_id].buffer + sizeof(char), &message_received, sizeof(shutdown_message));
    pthread_cond_signal(&threads[client_id].cond);
    return 0;
}

int thread_mount(void *buffer){
    mount_message message_received;
    int session_id;
    memcpy(&session_id, buffer + sizeof(char), sizeof(int));
    memcpy(&message_received, buffer + sizeof(char) + sizeof(int), sizeof(mount_message));

    int c_pipe = open(message_received.pipename, O_WRONLY);
    if(c_pipe == -1)
        return -1;
    clients[session_id].session_id = session_id;
    memcpy(clients[session_id].client_pipename, message_received.pipename, strlen(message_received.pipename));       
    clients[session_id].fd_client_pipe = c_pipe;

    if(write_function(c_pipe, &session_id, sizeof(int)) == -1)
        return -1;
    return 0;
}

int thread_unmount(void *buffer) {
    unmount_message message_received;
    memcpy(&message_received, buffer + sizeof(char), sizeof(unmount_message));

    int index = message_received.session_id;
    online_clients --;
    free_sessions[index] = FREE;
    clients[index].session_id = -1;
    memset(clients[index].client_pipename, '\0', MAX_SIZE_PIPENAME);
    if(close(clients[index].fd_client_pipe) == -1)
        return -1;
    return 0;
}

int thread_open(void *buffer){
    open_message message_received;
    memcpy(&message_received, buffer + sizeof(char), sizeof(open_message));
    
    int fd = tfs_open(message_received.name, message_received.flags);

    int client_pipe = clients[message_received.session_id].fd_client_pipe;
    if(write_function(client_pipe, &fd, sizeof(int)) == -1)
        return -1;

    return 0;
}

int thread_close(void *buffer){
    close_message message_received;

    memcpy(&message_received, buffer + sizeof(char), sizeof(close_message));

    int value = tfs_close(message_received.fhandle);
    int client_pipe = clients[message_received.session_id].fd_client_pipe;

    if(write_function(client_pipe, &value, sizeof(int)) == -1)
        return -1;
    return 0;
}

int thread_write(void *buffer){
    write_read_message message_received;
    
    memcpy(&message_received, buffer + sizeof(char), sizeof(write_read_message));
    void *buffer_write[message_received.len];
    memcpy(buffer_write, buffer + sizeof(char) + sizeof(write_read_message), message_received.len);

    ssize_t value = tfs_write(message_received.fhandle, buffer_write, message_received.len);
    int client_pipe = clients[message_received.session_id].fd_client_pipe;

    if(write_function(client_pipe, &value, sizeof(ssize_t)) == -1)
        return -1;
    return 0;
}

int thread_read(void *buffer){
    write_read_message message_received;
    memcpy(&message_received, buffer + sizeof(char), sizeof(write_read_message));

    int client_pipe = clients[message_received.session_id].fd_client_pipe;
    size_t len = message_received.len;
    void *buffer_read = malloc(len);

    size_t value = (size_t)tfs_read(message_received.fhandle, buffer_read, len);

    size_t to_write_size = sizeof(ssize_t) + value;
    void *to_write = (void*)malloc(to_write_size);
    memcpy(to_write, &value, sizeof(ssize_t));
    memcpy(to_write + sizeof(ssize_t), buffer_read, value);

    if(write_function(client_pipe, to_write, to_write_size) == -1)
        return -1;
    free(to_write);
    free(buffer_read);
    return 0;
}

int thread_shutdown(void *buffer){
    shutdown_message message_received;
    memcpy(&message_received, buffer + sizeof(char), sizeof(shutdown_message));

    int client_pipe = clients[message_received.session_id].fd_client_pipe;
    int value = tfs_destroy_after_all_closed();

    if(write_function(client_pipe, &value, sizeof(int)) == -1)
        return -1;

    if(value == 0)
        exit(0);

    return 0;
}

void *thread_function(void *arg){
    int index = *((int*)arg);
    char code;

    while(1){
        pthread_mutex_lock(&threads[index].mutex_lock);
        while(threads[index].buffer == NULL){
            pthread_cond_wait(&threads[index].cond, &threads[index].mutex_lock);
        }
        pthread_mutex_unlock(&threads[index].mutex_lock);
        memcpy(&code, threads[index].buffer, sizeof(char));
        int op_code = atoi(&code);
        if(op_code == TFS_OP_CODE_MOUNT){
            thread_mount(threads[index].buffer);
        }
        else if(op_code == TFS_OP_CODE_UNMOUNT){
            thread_unmount(threads[index].buffer); 
        }
        else if(op_code == TFS_OP_CODE_OPEN){
            thread_open(threads[index].buffer);
        }
        else if(op_code == TFS_OP_CODE_CLOSE){
            thread_close(threads[index].buffer);
        }
        else if(op_code == TFS_OP_CODE_WRITE){
            thread_write(threads[index].buffer);
        }
        else if(op_code == TFS_OP_CODE_READ){
            thread_read(threads[index].buffer);
        }
        else if(op_code == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            thread_shutdown(threads[index].buffer);
        }
        free(threads[index].buffer);
        threads[index].buffer = NULL;
    }
    return 0;
}

int main(int argc, char **argv) {
    char opcode;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    memcpy(server_pipename, pipename, strlen(pipename));
    
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipename);

    unlink(pipename); 
    if(mkfifo(pipename, 0777) != 0)
        return -1;
        
    sv_pipe = open(pipename, O_RDONLY);
    if(sv_pipe < 0)
        return -1;

    for(int i = 0; i < S; i++){
        free_sessions[i] = FREE;
        clients[i].session_id = -1;
        memset(clients[i].client_pipename, '\0', MAX_SIZE_PIPENAME);
        if(pthread_mutex_init(&threads[i].mutex_lock, NULL) == -1)
            return -1;
        if(pthread_cond_init(&threads[i].cond, NULL) == -1)
            return -1;
        threads[i].buffer = NULL;
        threads[i].id = i;
        if(pthread_create(&threads[i].tid, NULL, &thread_function, (void*)&threads[i].id) == -1)
            return -1;
    }

    if(tfs_init() != 0)
        return -1;

    while(1){
        if(read_function( &opcode, sizeof(char)) == -1)
            return -1;
        int op_code = atoi(&opcode);

        if(op_code == TFS_OP_CODE_MOUNT){
            if(server_handle_mount_call() ==-1)
                return -1;
        }
        else if(op_code == TFS_OP_CODE_UNMOUNT){
            if(server_handle_unmount_call() ==-1)
                return -1;
        }
        else if(op_code == TFS_OP_CODE_OPEN){
            if(server_handle_open_call() ==-1)
                return -1;
        }
        else if(op_code == TFS_OP_CODE_CLOSE){
            if(server_handle_close_call() ==-1)
                return -1;
        }
        else if(op_code == TFS_OP_CODE_WRITE){
            if(server_handle_write_call() == -1)
                return -1;
        }
        else if(op_code == TFS_OP_CODE_READ){
            if(server_handle_read_call() == -1)
                return -1;
        }
        else if(op_code == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            if(server_handle_shutdown_call() == -1)
                return -1;
        }
    }
    return 0;
}