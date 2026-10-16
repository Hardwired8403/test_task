#include "statdata.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(long) >= sizeof(int64_t), "long must hold a signed 64-bit id");
_Static_assert(sizeof(int) == sizeof(int32_t), "int must be 32 bits");
_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 &&
                   FLT_MANT_DIG == 24,
               "float must be float32");

static const unsigned char dump_magic[8] = {'S', 'T', 'D', 'A', 'T', 'A', '0', '1'};
static const uint32_t dump_version = 1;
static const size_t header_size = 20;
static const size_t record_size = 17;

static void encode_u32(unsigned char *dst, uint32_t value)
{
    dst[0] = (unsigned char)value;
    dst[1] = (unsigned char)(value >> 8);
    dst[2] = (unsigned char)(value >> 16);
    dst[3] = (unsigned char)(value >> 24);
}

static uint32_t decode_u32(const unsigned char *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static void encode_u64(unsigned char *dst, uint64_t value)
{
    for (size_t i = 0; i < 8; ++i)
        dst[i] = (unsigned char)(value >> (8 * i));
}

static uint64_t decode_u64(const unsigned char *src)
{
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i)
        value |= (uint64_t)src[i] << (8 * i);
    return value;
}

static int valid_data(const StatData *data, size_t count)
{
    if (count != 0 && data == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (count > SIZE_MAX / sizeof(*data)) {
        errno = EOVERFLOW;
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(data[i].cost)) {
            errno = EINVAL;
            return 0;
        }
    }
    return 1;
}

int StoreDump(const char *path, const StatData *data, size_t count)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (count > STATDATA_MAX_RESULT_RECORDS) {
        errno = E2BIG;
        return -1;
    }
    if (!valid_data(data, count))
        return -1;
    if (sizeof(long) < sizeof(int64_t) || sizeof(int) < sizeof(int32_t) ||
        (uintmax_t)count > UINT64_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return -1;

    unsigned char header[20];
    memcpy(header, dump_magic, sizeof(dump_magic));
    encode_u32(header + 8, dump_version);
    encode_u64(header + 12, (uint64_t)count);

    int status = 0;
    if (fwrite(header, 1, header_size, file) != header_size)
        status = -1;

    for (size_t i = 0; status == 0 && i < count; ++i) {
        unsigned char record[17];
        encode_u64(record, (uint64_t)(int64_t)data[i].id);
        encode_u32(record + 8, (uint32_t)(int32_t)data[i].count);
        uint32_t cost_bits;
        memcpy(&cost_bits, &data[i].cost, sizeof(cost_bits));
        encode_u32(record + 12, cost_bits);
        record[16] = (unsigned char)((data[i].primary & 1U) |
                                     ((data[i].mode & 7U) << 1));
        if (fwrite(record, 1, record_size, file) != record_size)
            status = -1;
    }

    if (fclose(file) != 0)
        status = -1;
    if (status != 0 && errno == 0)
        errno = EIO;
    return status;
}

int LoadDump(const char *path, StatData **data, size_t *count)
{
    if (path == NULL || data == NULL || count == NULL) {
        errno = EINVAL;
        return -1;
    }
    *data = NULL;
    *count = 0;

    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    unsigned char header[20];
    int status = -1;
    StatData *records = NULL;
    if (fread(header, 1, header_size, file) != header_size) {
        errno = ferror(file) ? EIO : EILSEQ;
        goto done;
    }
    if (memcmp(header, dump_magic, sizeof(dump_magic)) != 0 ||
        decode_u32(header + 8) != dump_version) {
        errno = EILSEQ;
        goto done;
    }

    uint64_t disk_count = decode_u64(header + 12);
    if (disk_count > STATDATA_MAX_RESULT_RECORDS) {
        errno = E2BIG;
        goto done;
    }
    if (disk_count > SIZE_MAX / sizeof(*records)) {
        errno = EOVERFLOW;
        goto done;
    }
    size_t record_count = (size_t)disk_count;
    records = record_count == 0 ? NULL : malloc(record_count * sizeof(*records));
    if (record_count != 0 && records == NULL)
        goto done;

    for (size_t i = 0; i < record_count; ++i) {
        unsigned char record[17];
        if (fread(record, 1, record_size, file) != record_size) {
            errno = ferror(file) ? EIO : EILSEQ;
            goto done;
        }

        int64_t id = (int64_t)decode_u64(record);
        int32_t count_value = (int32_t)decode_u32(record + 8);
        uint32_t cost_bits = decode_u32(record + 12);
        uint32_t flags = record[16];
        if (flags > 0x0fU) {
            errno = EILSEQ;
            goto done;
        }
        records[i].id = (long)id;
        records[i].count = (int)count_value;
        memcpy(&records[i].cost, &cost_bits, sizeof(cost_bits));
        records[i].primary = flags & 1U;
        records[i].mode = (flags >> 1) & 7U;
        if (!isfinite(records[i].cost)) {
            errno = EILSEQ;
            goto done;
        }
    }

    if (fgetc(file) != EOF) {
        errno = EILSEQ;
        goto done;
    }
    if (ferror(file)) {
        errno = EIO;
        goto done;
    }

    *data = records;
    *count = record_count;
    records = NULL;
    status = 0;

done:
    free(records);
    if (fclose(file) != 0 && status == 0) {
        free(*data);
        *data = NULL;
        *count = 0;
        status = -1;
    }
    return status;
}

static uint64_t hash_id(long id)
{
    uint64_t value = (uint64_t)(int64_t)id;
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

static int add_record(StatData *result, size_t *slots, size_t mask,
                      size_t *result_count, const StatData *record)
{
    size_t slot = (size_t)hash_id(record->id) & mask;
    while (slots[slot] != 0) {
        StatData *existing = &result[slots[slot] - 1];
        if (existing->id == record->id) {
            if ((record->count > 0 && existing->count > INT_MAX - record->count) ||
                (record->count < 0 && existing->count < INT_MIN - record->count)) {
                errno = EOVERFLOW;
                return -1;
            }
            float cost = existing->cost + record->cost;
            if (!isfinite(cost)) {
                errno = EOVERFLOW;
                return -1;
            }
            existing->count += record->count;
            existing->cost = cost;
            existing->primary = existing->primary && record->primary;
            if (record->mode > existing->mode)
                existing->mode = record->mode;
            return 0;
        }
        slot = (slot + 1) & mask;
    }

    size_t index = (*result_count)++;
    result[index] = *record;
    slots[slot] = index + 1;
    return 0;
}

int JoinDump(const StatData *left, size_t left_count,
             const StatData *right, size_t right_count,
             StatData **result, size_t *result_count)
{
    if (result == NULL || result_count == NULL) {
        errno = EINVAL;
        return -1;
    }
    *result = NULL;
    *result_count = 0;
    if (left_count > STATDATA_MAX_INPUT_RECORDS ||
        right_count > STATDATA_MAX_INPUT_RECORDS) {
        errno = E2BIG;
        return -1;
    }
    if (!valid_data(left, left_count) || !valid_data(right, right_count))
        return -1;

    if (left_count > SIZE_MAX - right_count) {
        errno = EOVERFLOW;
        return -1;
    }
    size_t total = left_count + right_count;
    if (total == 0)
        return 0;
    if (total > SIZE_MAX / sizeof(StatData) || total > SIZE_MAX / 2) {
        errno = EOVERFLOW;
        return -1;
    }

    size_t capacity = 1;
    while (capacity < total * 2) {
        if (capacity > SIZE_MAX / 2) {
            errno = EOVERFLOW;
            return -1;
        }
        capacity *= 2;
    }

    StatData *joined = malloc(total * sizeof(*joined));
    size_t *slots = calloc(capacity, sizeof(*slots));
    if (joined == NULL || slots == NULL) {
        free(joined);
        free(slots);
        return -1;
    }

    size_t joined_count = 0;
    for (size_t i = 0; i < left_count; ++i) {
        if (add_record(joined, slots, capacity - 1, &joined_count, &left[i]) != 0)
            goto fail;
    }
    for (size_t i = 0; i < right_count; ++i) {
        if (add_record(joined, slots, capacity - 1, &joined_count, &right[i]) != 0)
            goto fail;
    }

    free(slots);
    *result = joined;
    *result_count = joined_count;
    return 0;

fail:
    free(slots);
    free(joined);
    return -1;
}

static int compare_cost(const void *left, const void *right)
{
    const StatData *a = left;
    const StatData *b = right;
    int a_nan = isnan(a->cost);
    int b_nan = isnan(b->cost);
    if (a_nan || b_nan)
        return a_nan - b_nan;
    return (a->cost > b->cost) - (a->cost < b->cost);
}

void SortDump(StatData *data, size_t count)
{
    if (data != NULL && count > 1)
        qsort(data, count, sizeof(*data), compare_cost);
}
