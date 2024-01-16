#include <stdio.h>
#include <Windows.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>

unsigned int flag1 = 0;
unsigned int flag2 = 0;

#define MAX_BUFFER_SIZE 1024
unsigned int queue1[MAX_BUFFER_SIZE];
unsigned int queue2[MAX_BUFFER_SIZE];
int queueSize1 = 0; 
int queueSize2 = 0;

pthread_mutex_t lock1;
pthread_mutex_t lock2;


// 全局变量，信号量句柄
HANDLE semaphoreHandle;

// 初始化信号量
void initializeSemaphore() {
    semaphoreHandle = CreateSemaphore(NULL, 0, 1, NULL);
    if (semaphoreHandle == NULL) {
        printf("Failed to create semaphore.\n");
        exit(1);
    }
}


// 线程函数1
DWORD WINAPI ThreadFunction1(LPVOID lpParam){

    pthread_mutex_lock(&lock1);
    asm volatile(
        "movl $0, %%eax\n"  // 写指令

        "movl %%eax, %[addr]\n"  // 将写地址存储到全局变量
        "addl $1, %[size]\n"
        "movl $1, %[flag1]\n"
        : [addr] "=m" (queue1[queueSize1]),[size] "+m" (queueSize1),[flag1] "=m" (flag1) // 输出操作数：全局变量
        :  // 没有输入操作数
        : "eax","rcx", "rax" // 受影响的寄存器
    );
    pthread_mutex_unlock(&lock1);

    printf("flag1: %d\n", flag1);

    WaitForSingleObject(semaphoreHandle, INFINITE);

    pthread_mutex_lock(&lock1);
    asm volatile(
        /*
        "1:\n"
            "movl %[flag1], %%eax\n"  // 将flag1的值加载到寄存器eax
        "cmpl $0, %%eax\n"  // 将寄存器eax的值与0进行比较
        "jne 1b\n"  // 如果不等于0，跳转到标签1处
        */
        "movl $3, %%eax\n"  // 写指令
        "movl %%eax, %[addr]\n"  // 将写数据存储到全局变量
        "addl $1, %[size]\n"    
        "movl $1, %[flag1]\n"

        : [addr] "=m" (queue1[queueSize1]),[size] "+m" (queueSize1),[flag1] "=m" (flag1) // 输出操作数：全局变量
        :  // 没有输入操作数
        : "eax","rcx", "rax" // 受影响的寄存器
    );
    pthread_mutex_unlock(&lock1);
    printf("线程 %u 在运行\n", GetCurrentThreadId());

    return 0;
}
// 线程函数2
DWORD WINAPI ThreadFunction2(LPVOID lpParam){
    pthread_mutex_lock(&lock2);
    asm(
        "movl $0, %%eax\n"  // 写指令
        "movl %%eax, %[addr]\n"  // 将写地址存储到全局变量
        "addl $1, %[size]\n"
        "movl $1, %[flag2]\n"

        : [addr] "=m" (queue2[queueSize2]),[size] "+m" (queueSize2),[flag2] "=m" (flag2) // 输出操作数：全局变量
        :  // 没有输入操作数
        : "eax","rcx", "rax" // 受影响的寄存器
    );
    pthread_mutex_unlock(&lock2);

    printf("flag2: %d\n", flag2);

    WaitForSingleObject(semaphoreHandle, INFINITE);

    pthread_mutex_lock(&lock2);
    asm(
        /*
        "1:\n"
        "movl %[flag2], %%eax\n"  // 将flag1的值加载到寄存器eax
        "cmpl $0, %%eax\n"  // 将寄存器eax的值与0进行比较
        "jne 1b\n"  // 如果不等于0，跳转到标签1处
        */    
        "movl $2, %%eax\n"  // 写指令
        "movl %%eax, %[addr]\n"  // 将写地址存储到全局变量
        "addl $1, %[size]\n"
        "movl $1, %[flag2]\n"

        : [addr] "=m" (queue2[queueSize2]),[size] "+m" (queueSize2),[flag2] "=m" (flag2) // 输出操作数：全局变量
        :  // 没有输入操作数
        : "eax","rcx", "rax" // 受影响的寄存器
    );
    pthread_mutex_unlock(&lock2);

    printf("线程 %u 在运行\n", GetCurrentThreadId());
    return 0;
}

int main() {

    HANDLE hThread1, hThread2;
    initializeSemaphore();  // 初始化信号量

    // 创建线程1，并设置运行在第一个核心上
    DWORD_PTR thread1AffinityMask = 1 << 0;  // 第一个核心的掩码
    hThread1 = CreateThread(NULL, 0, ThreadFunction1, NULL, 0, NULL);
    SetThreadAffinityMask(hThread1, thread1AffinityMask);

    // 创建线程2，并设置运行在第二个核心上
    DWORD_PTR thread2AffinityMask = 1 << 1;  // 第二个核心的掩码
    hThread2 = CreateThread(NULL, 0, ThreadFunction2, NULL, 0, NULL);
    SetThreadAffinityMask(hThread2, thread2AffinityMask);

    while (flag1==1&&flag2==1) {
        //循环比较两个队列的数据
        for (int i = 0; i < queueSize1; i++)
        {
            if (queue1[i] != queue2[i])
            {
                printf("error\t");  //回卷
                printf("queue1:%d ", queue1[i]);
                printf("queue2:%d ", queue2[i]);
                break;
            }else
            {
                printf("%d\n", queue1[i]);
                pthread_mutex_lock(&lock1);
                flag1=0;
                pthread_mutex_unlock(&lock1);
                ReleaseSemaphore(semaphoreHandle, 1, NULL);// 通知线程1，队列1的数据已经处理完毕

                pthread_mutex_lock(&lock2);
                flag2=0;
                ReleaseSemaphore(semaphoreHandle, 1, NULL);
                pthread_mutex_unlock(&lock2);
              
            }
        }
        
    }
    // 等待线程结束
    WaitForSingleObject(hThread1, INFINITE);
    WaitForSingleObject(hThread2, INFINITE);


    // 关闭线程句柄
    CloseHandle(hThread1);
    CloseHandle(hThread2);
    // 关闭信号量句柄
    CloseHandle(semaphoreHandle);

    return 0;
}