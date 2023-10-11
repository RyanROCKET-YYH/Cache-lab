#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "cachelab.h"
#include <getopt.h>

csim_stats_t *stats;
unsigned long timeStamp = 0;

typedef struct {
    unsigned valid;
    unsigned dirty;
    unsigned long tag;
    unsigned long lru;
} Cacheline;

typedef struct {
    Cacheline *lines;
    unsigned long line_index;
} CacheSet;

typedef struct {
    CacheSet *sets;
} Cache;

Cache *cache_simulator;

void initCache(unsigned long s, unsigned long E, unsigned long b) {
    cache_simulator = (Cache *)malloc(sizeof(Cache));
    if (cache_simulator == NULL) {
        fprintf(stderr, "Memory allocation failed for cache_simulator!\n");
        exit(1);
    }

    unsigned long no_sets = 1 << s;
    cache_simulator->sets = (CacheSet *)malloc(no_sets * sizeof(CacheSet));
    if (cache_simulator->sets == NULL) {
        fprintf(stderr, "Memmory allocation failed for cache_simulator!\n");
        exit(1);
    }

    for (unsigned long i = 0; i < no_sets; i++) {
        cache_simulator->sets[i].lines = (Cacheline *)malloc(E * sizeof(Cacheline));
        cache_simulator->sets[i].line_index = 0;
       
        if (cache_simulator->sets[i].lines == NULL) {
            fprintf(stderr, "Memmory allocation failed for cache_simulator!\n");
            exit(1);
        }

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
    line->lru = timeStamp++;
    if (Op == 'S') {
        line->dirty = 1;
    } else {
        line->dirty = 0;
    }
}

unsigned long line2replace(CacheSet *set, unsigned long lines_no) {
    unsigned long lru_index = 0;
    unsigned long min_lru = set->lines[0].lru;
    for (unsigned long i = 0; i < lines_no; i++) {
        if (min_lru > set->lines[i].lru) {
            min_lru = set->lines[i].lru;
            lru_index = i;
        }
    }
    return lru_index;
}

void update_cache(unsigned long address, unsigned long set_bits, unsigned long block_bits, unsigned long lines_no, char Op, bool verbose) {
   // transform address into set and tag
    unsigned long set_index = (address >> block_bits) & (((unsigned long) -1) >> (64 - set_bits));
    unsigned long tag_index = (address >> (block_bits + set_bits));
    if (set_bits == 0){
        set_index = 0;
    }
    CacheSet *target_set = &(cache_simulator->sets[set_index]);
    bool hit = false;
    for (unsigned long i = 0; i < lines_no; i++) {
        Cacheline *line = &(target_set->lines[i]);
        if (line->valid == 1 && line->tag == tag_index) {
            stats->hits++;
            line->lru = timeStamp++;
            hit = true;
            if (Op == 'S') {
                if (line->dirty != 1) {
                    line->dirty = 1;
                    stats->dirty_bytes += (1 << block_bits);
                }
            }
            if (verbose) {
                printf(" hit dirty_bytes:%lu\n", stats->dirty_bytes);
            }
            break;
        }
    }
    
    if (!hit) {
        stats->misses++;
        if ((target_set->line_index) < lines_no) {
            Cacheline *line = &(target_set->lines[target_set->line_index]);
            update_cacheline(line, tag_index, Op);
            if (line->dirty == 1) {
                stats->dirty_bytes += (1 << block_bits);
            }
            target_set->line_index++;
            if (verbose) {
                printf(" miss dirty_bytes:%lu\n", stats->dirty_bytes);
            }
        } else {
            unsigned long lru_index = line2replace(target_set, lines_no);
            if (target_set->lines[lru_index].dirty == 1) {
                stats->dirty_evictions += (1 << block_bits);
                stats->dirty_bytes -= (1 << block_bits);
            } 
            update_cacheline(&(target_set->lines[lru_index]), tag_index, Op);
            if (target_set->lines[lru_index].dirty == 1) {
                stats->dirty_bytes += (1 << block_bits);
            }
            target_set->lines[lru_index].lru = timeStamp++;
            stats->evictions++;
            if (verbose) {
                printf(" miss evicition dirty_bytes:%lu evicted:%lu\n", stats->dirty_bytes, stats->dirty_evictions);
            }
        }
    }
}

/** Process a memory-access trace file. *
 * @param trace Name of the trace file to process.
 * @return 0 if successful, 1 if there were errors.
 */
int process_trace_file(const char *trace, unsigned long set_bits, unsigned long block_bits, unsigned long lines_no, bool verbose) {
    FILE *tfp = fopen(trace, "rt"); 
    if (!tfp) {
        fprintf(stderr, "Error opening trace file1\n");
        return 1;
    }
    size_t LINELEN = 25; // 1+1+16+4+1+1+1 = 25
    char linebuf[LINELEN]; // How big should LINELEN be? 
    int parse_error = 0;
    while (fgets(linebuf, (int)LINELEN, tfp)) {
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
        unsigned long size = strtoul(Size, &endptr, 10);
        if (Size == endptr || *endptr != '\0') {
            fprintf(stderr, "Error reading trace file-size\n");
            parse_error = 1;
            break;
        }
        if (verbose) {
            printf("%c %lx,%lu", Op, address, size);
        }
        update_cache(address, set_bits, block_bits, lines_no, Op, verbose);
    }
    fclose(tfp);
    return parse_error;
}

void helpMessage(void) {
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
    bool verbose = false;
    int s_flag=0, b_flag=0, E_flag=0, t_flag=0;
    unsigned long set_bits=0, block_bits=0,lines_no=0;
    char *trace = NULL;
    stats = (csim_stats_t *)malloc(sizeof(csim_stats_t));
    if (stats == NULL) {
        fprintf(stderr, "Memory allocation failed for stats!\n");
        exit(1);
    }

    stats->hits = 0;
    stats->misses = 0;
    stats->evictions = 0;
    stats->dirty_bytes = 0;
    stats->dirty_evictions = 0;

    while ((opt = getopt(argc, argv, "hvs:b:E:t:")) != -1) {
        switch (opt) {
            case 'h':
                helpMessage();
                exit(0);
            case 'v':
                verbose = true;
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

    printf("s:%lu, E:%lu, b:%lu\n", set_bits, lines_no, block_bits);
    initCache(set_bits, lines_no, block_bits);
    process_trace_file(trace, set_bits, block_bits, lines_no, verbose);
    freeCache(set_bits);
    printSummary(stats);
    free(stats);
    return 0;
}
