#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

#define MAX_BUF 500
char teststr[] = "The quick brown fox jumped over the lazy dog.";
char teststr2[] = "Thelazy dog.\n22";
char buf[MAX_BUF];

static int test_open(const char *path, mode_t mode, int flags) {
    int fd;
    fd = open(path, mode, flags);
    printf("* open() got fd %d\n", fd);
    if (fd < 0) {
        printf("ERROR opening file: %s\n", strerror(errno));
        exit(1);
    }
    return fd;
}

static int test_write(int fd, char* teststr, int teststrlen) {
    int r;
    printf("* writing test string\n");
    r = write(fd, teststr, teststrlen);
    printf("* wrote %d bytes\n", r);
    if (r < 0) {
            printf("ERROR writing file: %s\n", strerror(errno));
            exit(1);
    }
    return r;
}

static int test_read(int fd, char *buf, int buflen) {
    int i, r;
    i = 0;
    do  {
            printf("* attempting read of %d bytes\n", buflen -i);
            r = read(fd, &buf[i], buflen - i);
            printf("* read %d bytes\n", r);
            i += r;
    } while (i < buflen && r > 0);
    printf("* reading complete\n");
    if (r < 0) {
            printf("ERROR reading file: %s\n", strerror(errno));
            exit(1);
    }
    return r;
}

static int test_lseek(int fd, off_t offset, int whence) {
    int r;
    r = lseek(fd, offset, whence);
    if (r < 0) {
        printf("ERROR lseek: %s\n", strerror(errno));
        exit(1);
    }
    return r;
}

static int check_read(int k, int j, int r, char* buf1, char *buf2) {
    int max = j;
    do {
        if (buf1[k] != buf2[j]) {
            printf("ERROR  file contents mismatch\n");
            exit(1);
        }
        k++;
        j = (k + max)% r;
    } while (k < max);
    return 0;
}

static void test1(); {
    int fd, fd2, r;

    printf("\n**********\n* File Tester\n");

    snprintf(buf, MAX_BUF, "**********\n* write() works for stdout\n");
    write(1, buf, strlen(buf));
    snprintf(buf, MAX_BUF, "**********\n* write() works for stderr\n");
    write(2, buf, strlen(buf));

    printf("**********\n* opening new file \"test.file\"\n");
    fd = test_open("test.file", O_RDWR | O_CREAT, 0700); /* mode u=rw in octal */

    printf("* writing test string\n");
    test_write(fd, teststr, strlen(teststr));

    printf("* writing test string again\n");
    test_write(fd, teststr, strlen(teststr));

    printf("* closing file\n");
    close(fd);

    printf("**********\n* opening old file \"test.file\"\n");
    fd = test_open("test.file", O_RDONLY, 0);

    printf("* reading entire file into buffer \n");
    test_read(fd, buf, MAX_BUF);
    check_read(0, 0, strlen(teststr), buf, teststr);

    printf("**********\n* testing dup2\n");
    fd2 = 11;
    r = dup2(fd, fd2);
    if (r != 0) {
        printf("ERROR dup2: %s\n", strerror(errno));
        exit(1);
    }

    printf("**********\n* testing lseek\n");
    r = test_lseek(fd, 5, SEEK_SET);

    printf("**********\n* testing lseek on fd2\n");
    r = test_lseek(fd2, 5, SEEK_SET);

    printf("* reading 10 bytes of file into buffer \n");
    test_read(fd, buf, 10);
    check_read(0, 5, strlen(teststr), buf, teststr);

    printf("* file lseek  okay\n");
    printf("* closing file\n");
    close(fd);
}

int main(int argc, char * argv[]) {
    (void) argc;
    (void) argv;

    test1();    

    return 0;
}