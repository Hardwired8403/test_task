#ifndef STATDATA_H
#define STATDATA_H

#include <stddef.h>

#define STATDATA_MAX_INPUT_RECORDS 100000U
#define STATDATA_MAX_RESULT_RECORDS (2U * STATDATA_MAX_INPUT_RECORDS)

typedef struct StatData {
    long id;
    int count;
    float cost;
    unsigned int primary : 1;
    unsigned int mode : 3;
} StatData;

int StoreDump(const char *path, const StatData *data, size_t count);
int LoadDump(const char *path, StatData **data, size_t *count);
int JoinDump(const StatData *left, size_t left_count,
             const StatData *right, size_t right_count,
             StatData **result, size_t *result_count);
void SortDump(StatData *data, size_t count);

#endif
