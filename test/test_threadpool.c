#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void test(void *arg)
{
    char *temp = (char *)arg;
    int index;
    sscanf(temp, "this task[%d]", &index);
    int rest = (index + 1) * 1;
    printf("index %d\n", index);
    // while (rest > 0) {
        sleep(1);
    //     printf("%s res %d\n", temp, rest);
    //     rest--;
    // }
    printf("task[%d] exit\n", index);
    if (index == 4) temp[0] = '\0';
}

int main(void)
{
    int val = 0;
    printf("InitThreadPool\n");
    InitThreadPool(3);
    char *argv[5]; 
    for (int i = 0; i < 5; i++) {
        sleep(1);
        argv[i] = (char *)malloc(50);
        memset(argv[i], 0, 50);
        sprintf(argv[i], "this task[%d]",i);
        printf("push a task[%d]\n", val);
        val++;
        AddTaskInThreadPool(test, (void *)argv[i]);
    }
    while (1) {
        if (argv[4][0] == '\0') {
            break;
        }
    }
    printf("exit\n");
    DestroyThreadPool();
    AddTaskInThreadPool(test, (void *)argv[1]);
    printf("DestroyThreadPool exit\n");
    return 0;
}
