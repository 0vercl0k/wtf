#include <stdio.h>
#include <stdlib.h>

void
do_crash_test(
    char *input
    )
{
    if (input[0] == 'C' &&
        input[1] == 'R' &&
        input[2] == 'A' &&
        input[3] == 'S' &&
        input[4] == 'H') {
        *(char *)NULL = '\0';
    }
}

void
end_crash_test()
{
    printf("End crash test.\n");
}

int main(int argc, char* argv[])
{
    char *buf = NULL;
    size_t cbBuf = 10;
    ssize_t cbRead = 0;

    buf = (char *)calloc(1, cbBuf);
    if (!buf) {
        printf("calloc failed.\n");
        goto END;
    }

    printf("Enter some input.\n");
    cbRead = getline(&buf, &cbBuf, stdin);
    if (-1 == cbRead) {
        perror("getline failure: ");
        goto END;
    }

    do_crash_test(buf);

    end_crash_test();

END:
    if (buf) {
        free(buf);
    }
}
