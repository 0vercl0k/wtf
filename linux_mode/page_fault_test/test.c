// Jason Crowder - February 2024
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 0x1000
#define BUFF_SIZE (512 * 1024 * 1024)

void page_fault_test(void* p) {
    for (size_t i = 0; i < BUFF_SIZE; i += PAGE_SIZE) {
        char* pc = (char*)p + i;
        *pc = 'A';
    }
}

void done_with_test() { printf("Done with test.\n"); }

int main() {
    void* p = malloc(BUFF_SIZE);

    if (!p) {
        perror("malloc failed.\n");
        goto END;
    }

    printf("Press enter to do page fault test.\n");
    getchar();
    page_fault_test(p);
    done_with_test();

END:
    if (p) {
        free(p);
    }
}