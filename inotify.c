#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdlib.h>     /* 標準工具庫 (exit, malloc) */
#include <unistd.h>     /* Unix 標準函數 (read, close) */
#include <stdint.h>     /* 定義 uint32_t 等固定長度型別 */
#include <sys/inotify.h>/* Inotify 核心 API (inotify_init, inotify_event) */
#include <limits.h>     /* (選擇性) 若需要用到 NAME_MAX 等常數 */
#define BUFFER_SIZE 1000
#define CPY_BUFFER_SIZE 4096
#define FILENAME "AUTO.sav"
#define FILENAME_BAK "AUTO.sav.bak"

void mycopy_copy_data(int src_fd, int dest_fd, ssize_t file_size){
    ssize_t bytes_read, bytes_written;
    char buffer[CPY_BUFFER_SIZE];
    off_t current_pos = 0;
    off_t data_pos = 0;
    off_t dest_current_pos = 0;
    off_t hole_pos = 0;
    off_t next_bytes_read = 0;
    void *src_ptr, *dest_ptr;

    while(current_pos != file_size){
        data_pos = lseek(src_fd, current_pos, SEEK_DATA);
        if (data_pos == -1&& errno == ENXIO){
            ftruncate(dest_fd, file_size);
            break;
        }
        hole_pos = lseek(src_fd, data_pos, SEEK_HOLE);
        current_pos = lseek(src_fd, data_pos, SEEK_SET);

        next_bytes_read = hole_pos - current_pos > CPY_BUFFER_SIZE ?
                            CPY_BUFFER_SIZE : hole_pos - data_pos;

        
        //bytes_read = read(src_fd, buffer, next_bytes_read);
        src_ptr = mmap(NULL, next_bytes_read, PROT_READ, MAP_SHARED , src_fd , current_pos);
        if (!src_ptr){
            perror("failed to read");
            exit(1);}

        //bytes_written = write(dest_fd, buffer, next_bytes_read);
        dest_ptr = mmap(NULL, next_bytes_read, PROT_WRITE, MAP_SHARED , dest_fd , current_pos);

        if (!dest_ptr){
            perror("failed to write");
            exit(1);}

        memcpy(dest_ptr, src_ptr, next_bytes_read);
        current_pos += next_bytes_read;
    }
}

void mycopy(char* src, char* dest){
    int src_fd, dest_fd;
    ssize_t file_size;

    src_fd = open(src, O_RDONLY);
    dest_fd = open(dest, O_RDWR| O_CREAT | O_TRUNC, 0644);

    if (src_fd == -1 || dest_fd == -1){
        perror("open file error");
        exit(1);
    }

    file_size = lseek(src_fd, 0, SEEK_END);
    ftruncate(dest_fd, file_size);
    mycopy_copy_data(src_fd, dest_fd, file_size);
    
    close(dest_fd);
    close(src_fd);
}

void inotify_loop(int fd){
    char buffer[BUFFER_SIZE];
    ssize_t read_bytes;
    int current_pos;
    while (1){
        current_pos = 0;
        read_bytes = read(fd, buffer, BUFFER_SIZE);
        while (current_pos < read_bytes){
            struct inotify_event *event = (struct inotify_event *)&buffer[current_pos];
            if (event->len &&
                event->mask & IN_MODIFY &&
                strcmp(FILENAME,event->name) == 0)
            {
                printf("檔案 %s 被修改。\n", event->name);
                mycopy(FILENAME, FILENAME_BAK);
            }
            current_pos+=sizeof(event)+event->len;

        }
    }
}
int main(int argc, char* argv[]){
    int fd, wd;

    /* 1. 初始化 inotify */
    fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }
    /* 2. 加入監控路徑：目前的目錄 (.)
	   監控：建立、刪除、修改 */
    wd = inotify_add_watch(fd, "SaveGames", IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd == -1) {
        printf("無法監控該目錄\n");
    }
    else {
        printf("正在監控目前的目錄事件...\n");
    }
    // 3. loop
    inotify_loop(fd);
}
