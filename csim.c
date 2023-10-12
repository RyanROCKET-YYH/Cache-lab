
/**
 * Cache Simulator  Author: Yuhong YAO
 * OVERVIEW: uses a
 * memory instruction to get the side effects for each memory access on cache.
 *
 * USAGE:
 * By using this program, you need to have a trace file that follows Op
 * Addr,Size and by passing command from terminal(can check using '-h' option)
 * -s <s> is the set index bit, -b <b> is the block bit, -E <E> is the line
 * number per set, -t is the path of trace file -v is verbose mode which can
 * tell you side effects.
 *
 * WHAT YOU NEED TO KNOW:
 * In this simulator, I used Cacheline, CacheSet, Cache as the data structure.
 * - Cacheline: Represents a line in the cache. Contains the valid bit
 * (determines if the block is valid), dirty bit (determines if the cache has
 * been written to memory), tag (identifier for the block), and LRU (global
 * timestamp determining the time it was written to the cache). Note: We do not
 * access the actual data in the address.
 * - CacheSet: Represents a set in the cache. Contains a pointer to cachelines
 * and a `line_index` to indicate if the set is full. When `line_index` reaches
 * the set size (provided as input), eviction occurs based on the LRU policy.
 * - Cache: The main cache structure that contains an array of CacheSets.
 * Memory allocation: this simulator has put the cache sturcture on the heap
 * during initialization as well as the stats for side effects, freed after use,
 * checked when malloc LRU implementation: using a global that the bigger LRU
 * is, the earlier that address is put on the cache; when eviction happens, it
 * will go through the LRU on that set, and replace with the smallest LRU Side
 * effects: it has hits, misses, evictions, dirty_bytes(based on the block
 * size), dirty_eviction(dirty bytes that has been evicted)
 *
 * IMPROVMENTS:
 * As you may noticed, LRU is a global timastamp that grows everytime an
 * instruction is given, so it may get out of control if you are out of unsigned
 * long range, maybe add a LRU_RESET_HANDLER can make the simulator better. Also
 * the size provided in the trace file assume you will not go beyond the block
 * range. this can also be an improvement.
 *
 * WHY THIS DESIGN:
 * The design follows the typical roadmap of interpreting an address within the
 * cache structure, determining hits, and handling misses. The goal was to
 * ensure readability and handle each scenario meticulously.
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Keeps track of cache statistics.
 *  defined in cachelab.h
 */
csim_stats_t *stats;

/** Global timestamp for LRU (Least Recently Used) policy.
 * It acts as a global counter, incremented every time an instruction happens.
 */
unsigned long timeStamp = 0;

/** Represents a single line in each set *
 *  It is a fundamental unit in a cacheset, has info of valid, dirty, tag, and
 * LRU for eviction.
 */
typedef struct {
    unsigned valid;    // determine if the block has valid bit
    unsigned dirty;    // determine if the block has been written
    unsigned long tag; // identifier for the block
    unsigned long lru; // lru time stamp, bigger number lastest updated
} Cacheline;

/** Represent a set in the cache. *
 *  A cacheset contains multiple arrays of cachelines.
 */
typedef struct {
    Cacheline *lines; // array of cachelines
    unsigned long
        line_index; // index of the next line to be used before eviction
} CacheSet;

/** Represent the whole cache. *
 * Contains an array of sets, which can be understood as the top level
 * sturcture.
 */
typedef struct {
    CacheSet *sets; // arrary of cachesets
} Cache;

/** Global cache simulator instance */
Cache *cache_simulator;

/** Initializes the cache structure.
 *
 * Allocates and initializes the cache simulator based on provided parameters.
 * Each cacheline and cacheset are initialized.
 *
 * @param s Number of set index bits.
 * @param E Number of lines per set.
 * @param b Number of block bits.
 */
void initCache(unsigned long s, unsigned long E, unsigned long b) {
    cache_simulator = (Cache *)malloc(sizeof(Cache));
    if (cache_simulator == NULL) {
        fprintf(stderr, "Memory allocation failed for cache_simulator!\n");
        exit(1);
    }

    unsigned long no_sets = 1 << s;
    cache_simulator->sets = (CacheSet *)malloc(no_sets * sizeof(CacheSet));
    if (cache_simulator->sets == NULL) {
        fprintf(stderr,
                "Memmory allocation failed for cache_simulator->sets!\n");
        exit(1);
    }

    for (unsigned long i = 0; i < no_sets; i++) {
        cache_simulator->sets[i].lines =
            (Cacheline *)malloc(E * sizeof(Cacheline));
        cache_simulator->sets[i].line_index = 0;

        if (cache_simulator->sets[i].lines == NULL) {
            fprintf(
                stderr,
                "Memmory allocation failed for cache_simulator->sets.lines!\n");
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

/** @brief Frees the memory allocated to the cache simulator.
 *
 * Deallocates the memory used by the cachelines and sets.
 *
 * @param set_bits Number of set index bits.
 */
void freeCache(unsigned long set_bits) {
    unsigned long no_sets = 1 << set_bits;
    for (unsigned long i = 0; i < no_sets; i++) {
        free(cache_simulator->sets[i].lines);
    }
    free(cache_simulator->sets);
    free(cache_simulator);
}

/** @brief Updates the cacheline with new data.
 *
 * Modifies a cacheline based on provided tag and operation. Happens when there
 * is a miss. It includes set the valid bit to 1, tag to desired tag, update the
 * lru and dirty bit when there is a write.
 *
 * @param line The cacheline to be updated.
 * @param tag_index The new tag value.
 * @param Op The operation being performed ('L' for load, 'S' for store).
 */
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

/** @brief Determines the cacheline to be replaced based on LRU.
 *
 * Searches for the least recently used cacheline in a cache set.
 * In each set, take the first line lru info and compare with the rest of
 * line.lru.
 *
 * @param set The cache set to search.
 * @param lines_no Number of lines in the cache set.
 * @return The index of the cacheline to replace.
 */
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

/** @brief Checks if the cache line is a hit.
 *
 * Searches through the target set for a valid line with a matching tag. If
 * found, updates the statistics and LRU timestamp.
 * @param target_set The set array matching the set_index.
 * @param tag_index  The identifier for each cache memory.
 * @param lines_no Total number of lines per set.
 * @param block_bits The number of block index bit from command.
 * @param Op The operation in the instruction.
 * @param verbose Flag of verbose outputs.
 *
 * @return True if hit, false if miss.
 */
bool detect_hit(CacheSet *target_set, unsigned long tag_index,
                unsigned long lines_no, unsigned long block_bits, char Op,
                bool verbose) {
    for (unsigned long i = 0; i < lines_no; i++) {
        Cacheline *line = &(target_set->lines[i]);
        if (line->valid == 1 && line->tag == tag_index) {
            stats->hits++;
            line->lru = timeStamp++;
            if (Op == 'S' && line->dirty != 1) {
                line->dirty = 1;
                stats->dirty_bytes += (1 << block_bits);
            }
            if (verbose) {
                printf(" hit dirty_bytes:%lu\n", stats->dirty_bytes);
            }
            return true;
        }
    }
    return false;
}

/** @brief Handle cold/compulsory misses.
 *
 * If the cache set is not full, it will start from the first line to the last
 * line in the set. So, this functions will handle this case and update stats.
 *
 * @param target_set The set array matching the set_index.
 * @param tag_index The identifier for each cache memory.
 * @param lines_no Total number of lines per set.
 * @param Op The operation in the instruction.
 * @param verbose Flag of verbose outputs.
 */
void detect_coldmiss(CacheSet *target_set, unsigned long tag_index,
                     unsigned long block_bits, char Op, bool verbose) {
    Cacheline *line = &(target_set->lines[target_set->line_index]);
    update_cacheline(line, tag_index, Op);
    if (line->dirty == 1) {
        stats->dirty_bytes += (1 << block_bits);
    }
    target_set->line_index++;
    if (verbose) {
        printf(" miss dirty_bytes:%lu\n", stats->dirty_bytes);
    }
}

/** @brief Handle cache misses that need eviction.
 *
 * When it is not a cold miss, it is either conflict or compulsory miss in the
 * cache which need to evict the least recently used cacheline. Update
 * dirty_eviction, dirty_bytes & eviction stats in the function.
 *
 * @param target_set he set array matching the set_index.
 * @param tag_index The identifier for each cache memory.
 * @param block_bits The number of block index bit from command.
 * @param lines_no Total number of lines per set.
 * @param Op The operation in the instruction.
 * @param verbose Flag of verbose outputs.
 */
void detect_othermiss(CacheSet *target_set, unsigned long tag_index,
                      unsigned long block_bits, unsigned long lines_no, char Op,
                      bool verbose) {
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
        printf(" miss evicition dirty_bytes:%lu evicted:%lu\n",
               stats->dirty_bytes, stats->dirty_evictions);
    }
}

/**
 * @brief Updates the cache upon a memory access.
 *
 * Handles cache hits, misses, and evictions based on the memory address and
 * operation. When each side effect happens, the stats will get updated.
 *
 * @param address The memory address being accessed.
 * @param set_bits The number of set index bit from command.
 * @param block_bits The number of block index bit from command.
 * @param lines_no Total number of lines per set.
 * @param Op The operation in the instruction.
 * @param verbose Flag of verbose outputs.
 */
void update_cache(unsigned long address, unsigned long set_bits,
                  unsigned long block_bits, unsigned long lines_no, char Op,
                  bool verbose) {
    // transform address into set and tag
    unsigned long set_index =
        (address >> block_bits) & (((unsigned long)-1) >> (64 - set_bits));
    unsigned long tag_index = (address >> (block_bits + set_bits));
    if (set_bits == 0) {
        set_index = 0;
    }
    CacheSet *target_set = &(cache_simulator->sets[set_index]);

    if (!detect_hit(target_set, tag_index, lines_no, block_bits, Op, verbose)) {
        stats->misses++;
        if ((target_set->line_index) < lines_no) {
            detect_coldmiss(target_set, tag_index, block_bits, Op, verbose);
        } else {
            detect_othermiss(target_set, tag_index, block_bits, lines_no, Op,
                             verbose);
        }
    }
}

/** Process a memory-access trace file. *
 * Update the cache memory.
 * @param trace Name of the trace file to process.
 * @param set_bits The number of set index bit from command.
 * @param block_bits The number of block index bit from command.
 * @param lines_no Total number of lines per set.
 * @param verbose Flag of verbose outputs. 
 * @return 0 if successful, 1 if there were errors.
 */
int process_trace_file(const char *trace, unsigned long set_bits,
                       unsigned long block_bits, unsigned long lines_no,
                       bool verbose) {
    FILE *tfp = fopen(trace, "rt");
    if (!tfp) {
        fprintf(stderr, "Error opening trace file\n");
        exit(1);
    }
    size_t LINELEN = 25;   // 1+1+16+4+1+1+1 = 25
    char linebuf[LINELEN]; // How big should LINELEN be?
    int parse_error = 0;
    while (fgets(linebuf, (int)LINELEN, tfp)) {
        // Parse the line of text in ’linebuf’.
        size_t len = strlen(linebuf);
        if (len == LINELEN - 1 && linebuf[len - 1] != '\n') {
            fprintf(stderr,
                    "Error reading trace file: line reads over threshold\n");
            exit(1);
        }

        char Op = linebuf[0]; // read Op which is always 1st bit
        char *Addr = strtok(&linebuf[2], ","); // address, terminate at ,
        char *Size =
            strtok(NULL, "\n\t\r "); // size, terminate at \n\t\r and space
        char *Junk = strtok(NULL, "\n\t\r ");
        if (!Op || !Addr || !Size) {
            fprintf(
                stderr,
                "Error reading trace file: missing element in instruction\n");
            exit(1);
        }
        if (Junk) {
            fprintf(stderr, "Unexpected junk in trace file: %s\n", Junk);
            exit(1);
        }
        if (Op != 'L' && Op != 'S') {
            fprintf(stderr, "Invalid operation in trace file\n");
            exit(1);
        }
        char *endptr;
        unsigned long address = strtoul(Addr, &endptr, 16);
        if (Addr == endptr || *endptr != '\0') {
            fprintf(stderr, "Error reading trace file-address\n");
            exit(1);
        }
        unsigned long size = strtoul(Size, &endptr, 10);
        if (Size == endptr || *endptr != '\0') {
            fprintf(stderr, "Error reading trace file-size\n");
            exit(1);
        }
        if (verbose) {
            printf("%c %lx,%lu", Op, address, size);
        }
        update_cache(address, set_bits, block_bits, lines_no, Op, verbose);
    }
    fclose(tfp);
    return parse_error;
}

/* Display the help message for cache simulator.*/
void helpMessage(void) {
    printf("Usage: ./csim -ref [-v] -s <s> -E <E> -b <b> -t <trace >\n");
    printf("       ./csim -ref -h\n");
    printf("     -h          Print this help message and exit\n");
    printf("     -v          Verbose mode: report effects of each memory "
           "operation\n");
    printf("     -s <s>      Number of set index bits (there are 2**s sets)\n");
    printf("     -b <b>      Number of block bits (there are 2**b blocks)\n");
    printf("     -E <E>      Number of lines per set (associativity)\n");
    printf("     -t <trace>  File name of the memory trace to process\n");
}

/** Main function: Entry point to the cache simulator.
 *
 * Parses command-line arguments, initializes structures, processes trace file.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return 0 for successful execution 1 for error.
 */
int main(int argc, char **argv) {
    int opt;
    bool verbose = false;
    int s_flag = 0, b_flag = 0, E_flag = 0, t_flag = 0;
    unsigned long set_bits = 0, block_bits = 0, lines_no = 0;
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
            // printf("number of set index bits is %lu\n",set_bits);
            break;
        case 'b':
            b_flag = 1;
            block_bits = strtoul(optarg, NULL, 10);
            // printf("number of block bits is %lu\n",block_bits);
            break;
        case 'E':
            E_flag = 1;
            lines_no = strtoul(optarg, NULL, 10);
            if (lines_no >= 0x7FFFFFFFFFFFFFFF) {
                printf("Failed to allocate memory\n");
                exit(1);
            }
            // printf("number of lines per set is %lu\n",lines_no);
            break;
        case 't':
            t_flag = 1;
            trace = optarg;
            // printf("Trace file is %s\n", optarg);
            break;
        case '?':
            if (optopt == 's' || optopt == 'b' || optopt == 'E' ||
                optopt == 't') {
                printf("Mandatory arguments missing or zero.\n");
                helpMessage();
                printf("\nThe -s, -b, -E, and -t options must be supplied for "
                       "all simulations.\n");
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
        printf("\nThe -s, -b, -E, and -t options must be supplied for all "
               "simulations.\n");
        exit(1);
    }

    if (block_bits >= 64 || set_bits >= 64 || set_bits + block_bits >= 64) {
        printf("Error: s + b is too large (s = %lu, b = %lu)\n", set_bits,
               block_bits);
        exit(1);
    }
    initCache(set_bits, lines_no, block_bits);
    process_trace_file(trace, set_bits, block_bits, lines_no, verbose);
    freeCache(set_bits);
    printSummary(stats);
    free(stats);
    return 0;
}
