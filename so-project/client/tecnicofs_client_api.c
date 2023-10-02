#include "tecnicofs_client_api.h"

int c_pipe, session_id, sv_pipe;
char c_pipename[MAX_SIZE_PIPENAME], server_pipename[MAX_SIZE_PIPENAME];

int read_function(void* message_received , size_t message_size) {
    size_t bytes_read = 0;
    ssize_t curr = 0;

    while(bytes_read < message_size){
        curr = read(c_pipe, message_received + bytes_read, message_size - bytes_read);
        if(curr == -1)
            return -1;
        bytes_read += (size_t)curr;
    }
    return 0;
}

int write_function(void* to_write, size_t to_write_size) {
    size_t bytes_written = 0;
    ssize_t curr = 0;

    while(bytes_written < to_write_size){
        curr = write(sv_pipe, to_write + bytes_written, to_write_size - bytes_written);
        if(curr == -1)
                 return -1;
        bytes_written += (size_t)curr;
    }
    return 0;
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    mount_message message;
    size_t message_size = sizeof(char) + sizeof(mount_message);
    void* to_send = (void*)malloc(message_size);

    unlink(client_pipe_path);
    if(mkfifo(client_pipe_path, 0777) == -1)
        return -1;

    memcpy(c_pipename, client_pipe_path, strlen(client_pipe_path));
    memcpy(server_pipename, server_pipe_path, strlen(server_pipe_path));
    
    sv_pipe = open(server_pipe_path, O_WRONLY);
    if(sv_pipe == -1)
        return -1;                

    //cria a mensagem a enviar ao servidor
    memset(message.pipename, '\0',MAX_SIZE_PIPENAME);
    memcpy(message.pipename, client_pipe_path, MAX_SIZE_PIPENAME);
    char op_code = '0' + TFS_OP_CODE_MOUNT;
    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(mount_message));

    if(write_function(to_send, message_size) == -1)
        return -1;

    c_pipe = open(client_pipe_path, O_RDONLY);
    if(c_pipe == -1)
        return -1;

    int value;
    if(read_function(&value, sizeof(int)) == -1)
        return -1;

    if(value == -1)
        return -1;
    else
        session_id = value;
        
    free(to_send);
    return 0;
}

int tfs_unmount(){
    unmount_message message;
    size_t message_size = sizeof(char) + sizeof(unmount_message);
    void* to_send = (void*)malloc(message_size);

    char op_code = '0' + TFS_OP_CODE_UNMOUNT;
    message.session_id = session_id;

    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(unmount_message));

    if(write_function(to_send, message_size) == -1)
        return -1;
        
    if(close(c_pipe) == -1 || close(sv_pipe) == -1 || unlink(c_pipename) == -1)
        return -1;

    return 0;
}

int tfs_open(char const *name, int flags) {
    open_message message;
    size_t message_size = sizeof(char) + sizeof(open_message);
    void* to_send = (void*)malloc(message_size);

    char op_code = '0' + TFS_OP_CODE_OPEN;
    message.session_id = session_id;
    memset(message.name, '\0', MAX_SIZE_PIPENAME);
    memcpy(message.name, name, MAX_SIZE_PIPENAME);
    message.flags = flags;

    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(open_message));

    if(write_function(to_send, message_size) == -1)
        return -1;

    int fd;
    
    if(read_function(&fd, sizeof(int)) == -1)
        return -1;
    
    free(to_send);
    return fd;
}

int tfs_close(int fhandle) {
    close_message message;
    size_t message_size = sizeof(char) + sizeof(close_message);
    void* to_send = (void*)malloc(message_size);

    char op_code = '0' + TFS_OP_CODE_CLOSE;
    message.session_id = session_id;
    message.fhandle = fhandle;

    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(close_message));

    if(write_function(to_send, message_size) == -1)
        return -1;
    
    int value;
    if(read_function(&value, sizeof(int)) == -1)
        return -1;

    free(to_send);
    return value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    write_read_message message;
    size_t message_size = sizeof(char) + sizeof(write_read_message) + len;
    void* to_send = (void*)malloc(message_size);

    char op_code = '0' + TFS_OP_CODE_WRITE;
    message.session_id = session_id;
    message.fhandle = fhandle;
    message.len = len;

    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(write_read_message));
    memcpy(to_send + sizeof(char) + sizeof(message), buffer, len);

    if(write_function(to_send, message_size) == -1)
        return -1;

    ssize_t value;
    if(read_function(&value, sizeof(ssize_t)) == -1)
        return -1;
        
    free(to_send);
    return value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    write_read_message message;
    size_t message_size = sizeof(char) + sizeof(write_read_message);
    void* to_send = (void*)malloc(message_size);

    char op_code = '0' + TFS_OP_CODE_READ;
    message.session_id = session_id;
    message.fhandle = fhandle;
    message.len = len;

    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(write_read_message));

    if(write_function(to_send, message_size) == -1)
        return -1; 

    ssize_t value;
    if(read_function(&value, sizeof(ssize_t)) == -1)
        return -1;  

    if(read_function(buffer, (size_t)value) == -1)
        return -1;

    return value;
}

int tfs_shutdown_after_all_closed() {
    shutdown_message message;
    size_t message_size = sizeof(char) + sizeof(shutdown_message);
    void* to_send = (void*)malloc(message_size);
    
    char op_code = '0' + TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    message.session_id = session_id;

    memcpy(to_send, &op_code, sizeof(char));
    memcpy(to_send + sizeof(char), &message, sizeof(shutdown_message));

    if(write_function(to_send, message_size) == -1)
        return -1;
    
    int value;
    if(read_function(&value, sizeof(int)) == -1)
        return -1;
    
    if(value == 0){
        if(close(sv_pipe) == -1 || close(c_pipe) == -1 || unlink(c_pipename))
            return -1;
    }

    return value;
}
