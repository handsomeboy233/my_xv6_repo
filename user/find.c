#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

char *fmtname2(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), '\0', DIRSIZ - strlen(p));
  return buf;
}

void find(char *path,char *purpose_dir){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "ls: cannot open %s\n", path);
        // printf("open error!!!");
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }
    
    switch (st.type) {
        case T_FILE:
            if (!strcmp(fmtname2(path),purpose_dir)){
                printf("%s\n",path);
            }
            break;
            
        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("ls: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                
                if (!strcmp(fmtname2(buf),purpose_dir)){
                    printf("%s\n",buf);
                }

                if ((strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)) {
                    
                    continue;
                }

                // if ((strcmp(buf, ".") == 0 || !strcmp(buf, "..") == 0)) {
                    
                //     continue;
                // }

                if ((st.type == T_DIR)&&((strcmp(buf,"\0")))) {
                    
                    find(buf, purpose_dir);
                    
                }

                }
            break;
            
    }
    close(fd);
}

int main(int argc,char* argv[]){
    //开始路径的文件夹
    char* start_dir;
    char* purpose_dir;
    // 如果提供了参数，则使用提供的目录；否则使用当前目录
    if (argc > 2){
        start_dir = argv[1];
        purpose_dir = argv[2];
        find(start_dir,purpose_dir);
    }else{
        printf("未提供路径 !!! error !!!");
    }
    exit(0);
}