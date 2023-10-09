#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void helpMessage() {
    printf("Usage: ./csim -ref [-v] -s <s> -E <E> -b <b> -t <trace >\n");
    printf("       ./csim -ref -h\n");
    printf("     -h          Print this help message and exit\n");
    printf("     -v          Verbose mode: report effects of each memory operation\n");
    printf("     -s <s>      Number of set index bits (there are 2**s sets)\n");
    printf("     -b <b>      Number of block bits (there are 2**b blocks)\n");
    printf("     -E <E>      Number of lines per set (associativity)\n");
    printf("     -t <trace>  File name of the memory trace to process\n");

}

int main(int argc, char **argv) {
    int opt;
    int s_flag=0, b_flag=0, E_flag=0, t_flag=0;
    unsigned long set_bits=0, block_bits=0,lines_no=0;

    while ((opt = getopt(argc, argv, "hvs:b:E:t:")) != -1) {
        switch (opt) {
            case 'h':
                helpMessage();
                exit(0);
            case 'v':
                printf("it si in verbose mode");
                break;
            case 's':
                s_flag = 1;
                set_bits = strtoul(optarg, NULL, 10);
                printf("number of set index bits is %lu\n",set_bits);
                break;
            case 'b':
                b_flag = 1;
                block_bits = strtoul(optarg, NULL, 10);
                printf("number of block bits is %lu\n",block_bits);
                break;
            case 'E':
                E_flag = 1;
                lines_no = strtoul(optarg, NULL, 10);
                if (lines_no >= 0x7FFFFFFFFFFFFFFF) {
                    printf("Failed to allocate memory\n");
                    exit(1);
                }
                printf("number of lines per set is %lu\n",lines_no);
                break;
            case 't':
                t_flag = 1;
                printf("Trace file is %s\n", optarg);
                break;
            case '?':
                if (optopt == 's' || optopt == 'b' || optopt == 'E' || optopt == 't') {
                    printf("Mandatory arguments missing or zero.\n");
                    helpMessage();
                    printf("\nThe -s, -b, -E, and -t options must be supplied for all simulations.\n");
                } else {
                    printf("Error while parsing arguments.\n");
                    helpMessage();
                }
                exit(1);
        }
    }

    if (!s_flag || !b_flag || !E_flag || !t_flag) {
        printf("Mandatory arguments missing or zero.\n");
        helpMessage();
        printf("\nThe -s, -b, -E, and -t options must be supplied for all simulations.\n");
        exit(1);
    }

    if (block_bits >= 64 || set_bits >= 64 || set_bits+block_bits>=64) {
        printf("Error: s + b is too large (s = %lu, b = %lu)\n",set_bits, block_bits);
        exit(1);
    } 

    
    return 0;
}