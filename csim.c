#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "cachelab.h"

unsigned long timestamp = 0;
csim_stats_t *stats = {0};

typedef struct {
    unsigned valid;
    unsigned dirty;
    unsigned long tag;
    unsigned long lru;
} Cacheline;

typedef struct {
    Cacheline *lines;
} CacheSet;

typedef struct {
    CacheSet *sets;
} Cache;

Cache *cache_simulator;

void initCache(unsigned long s, unsigned long E, unsigned long b) {
    cache_simulator = (Cache *)malloc(sizeof(Cache));

    unsigned long no_sets = 1 << s;
    cache_simulator->sets = (CacheSet *)malloc(no_sets * sizeof(CacheSet));

    for (unsigned long i = 0; i < no_sets; i++) {
        cache_simulator->sets[i].lines = (Cacheline *)malloc(E * sizeof(Cacheline));

        for (unsigned long j = 0; j < E; j++) {
            cache_simulator->sets[i].lines[j].valid = 0;
            cache_simulator->sets[i].lines[j].dirty = 0;
            cache_simulator->sets[i].lines[j].tag = 0;
            cache_simulator->sets[i].lines[j].lru = 0;
        }
    }
}

void freeCache(unsigned long set_bits) {
    unsigned long no_sets = 1 << set_bits;
    for (unsigned long i = 0; i < no_sets; i++) {
        free(cache_simulator->sets[i].lines);
    }
    free(cache_simulator->sets);
    free(cache_simulator);
}

void update_cacheline(Cacheline *line, unsigned long tag_index, char Op) {
    line->valid = 1;
    line->tag = tag_index;
    line->lru = timestamp++;
    if (Op == 'S') {
        line->dirty = 1;
    }
}

unsigned long line2replace(CacheSet *set, ) {
    return 0
}

void update_cache(unsigned long address, unsigned long set_bits, unsigned long block_bits, unsigned long lines_no, char Op) {
   // transform address into set and tag
    unsigned long set_index = (address >> block_bits) & (((unsigned long) -1) >> (64 - set_bits));
    unsigned long tag_index = (address >> (block_bits + set_bits));
    
    CacheSet *target_set = &(cache_simulator->sets[set_index]);

    bool hit = false;
    for (unsigned long i = 0; i < lines_no; i++) {
        Cacheline *line = &(target_set->lines[i]);
        if (line->valid == 1 && line->tag == tag_index) {
            stats.hits++;
            line->lru = timestamp++;
            hit = true;
            if (Op == 'S') {
                line->dirty = 1;
            }
            printf("$lu",stats->hits);
            break;
        }
    }
    
    if (!hit) {
        printf("no hit record\n");
    }
}

/** Process a memory-access trace file. *
 * @param trace Name of the trace file to process.
 * @return 0 if successful, 1 if there were errors.
 */
int process_trace_file(const char *trace, unsigned long set_bits, unsigned long block_bits, unsigned long lines_no) {
    FILE *tfp = fopen(trace, "rt"); 
    if (!tfp) {
        fprintf(stderr, "Error opening trace file1\n");
        return 1;
    }
    int LINELEN = 25; // 1+1+16+4+1+1+1 = 25
    char linebuf[LINELEN]; // How big should LINELEN be? 
    int parse_error = 0;
    while (fgets(linebuf, LINELEN, tfp)) {
        // Parse the line of text in ’linebuf’.
        size_t len = strlen(linebuf);
        if (len == LINELEN - 1 && linebuf[len - 1] != '\n') {
            fprintf(stderr, "Error reading trace file2\n");
            parse_error = 1;
            return parse_error;
        }

        char Op = linebuf[0];   // read Op which is always 1st bit
        char *Addr = strtok(&linebuf[2], ","); // address, terminate at ,
        char *Size = strtok(NULL, "\n\t\r "); // size, terminate at \n\t\r and space 
        char *Junk = strtok(NULL, "\n\t\r ");
        if (!Op || !Addr || !Size) {
            fprintf(stderr, "Error reading trace file3\n");
            parse_error = 1;
            break;
        } 
        if (Junk) {
            fprintf(stderr, "Unexpected junk in trace file: %s\n", Junk);
            parse_error = 1;
            break;
        }
        if (Op != 'L' && Op != 'S') {
            fprintf(stderr, "Invalid operation in trace file\n");
            parse_error = 1;
            break;
        } 
        char *endptr;
        unsigned long address = strtoul(Addr, &endptr, 16);
        if (Addr == endptr || *endptr != '\0') {
            fprintf(stderr, "Error reading trace file-address\n");
            parse_error = 1;
            break;
        }
        unsigned int size = strtoul(Size, &endptr, 10);
        if (Size == endptr || *endptr != '\0') {
            fprintf(stderr, "Error reading trace file-size\n");
            parse_error = 1;
            break;
        }

        printf("Op: %c, Addr: %lx, Size: %u\n", Op, address, size);
        update_cache(address, set_bits, block_bits, lines_no, Op);
    }
    fclose(tfp);

    return parse_error;
}

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
    char *trace = NULL;

    while ((opt = getopt(argc, argv, "hvs:b:E:t:")) != -1) {
        switch (opt) {
            case 'h':
                helpMessage();
                exit(0);
            case 'v':
                //printf("it si in verbose mode");
                break;
            case 's':
                s_flag = 1;
                set_bits = strtoul(optarg, NULL, 10);
                //printf("number of set index bits is %lu\n",set_bits);
                break;
            case 'b':
                b_flag = 1;
                block_bits = strtoul(optarg, NULL, 10);
                //printf("number of block bits is %lu\n",block_bits);
                break;
            case 'E':
                E_flag = 1;
                lines_no = strtoul(optarg, NULL, 10);
                if (lines_no >= 0x7FFFFFFFFFFFFFFF) {
                    printf("Failed to allocate memory\n");
                    exit(1);
                }
                //printf("number of lines per set is %lu\n",lines_no);
                break;
            case 't':
                t_flag = 1;
                trace = optarg;
                //printf("Trace file is %s\n", optarg);
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

    initCache(set_bits, lines_no, block_bits);
    process_trace_file(trace, set_bits, lines_no, block_bits);
    freeCache(set_bits);
    return 0;
}