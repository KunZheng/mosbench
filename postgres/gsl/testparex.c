#include <stdio.h>

#include "parex.h"

void p1(void)
{
        printf("A1\n");
        swtch();
        printf("A2\n");
        swtch();
        printf("A3\n");
}

void p2(void)
{
        printf("B1\n");
        swtch();
        printf("B2\n");
        swtch();
        printf("B3\n");
}

void newline(void)
{
        printf("\n");
}

int main(int argc, char **argv)
{
        procInit(newline, 1);

        procAdd("A", p1);
        procAdd("B", p2);

        procRun();

        return 0;
}
