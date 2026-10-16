#include "statdata.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void print_mode(unsigned int mode, char text[4])
{
    size_t length = 0;
    unsigned int value = mode;
    do {
        text[length++] = (char)('0' + (value & 1U));
        value >>= 1;
    } while (value != 0);
    for (size_t i = 0; i < length / 2; ++i) {
        char temp = text[i];
        text[i] = text[length - i - 1];
        text[length - i - 1] = temp;
    }
    text[length] = '\0';
}

static int print_records(const StatData *data, size_t count)
{
    size_t shown = count < 10 ? count : 10;
    if (puts("Идентификатор      Количество Стоимость Основной Режим") == EOF)
        return -1;
    for (size_t i = 0; i < shown; ++i) {
        char mode[4];
        print_mode(data[i].mode, mode);
        if (printf("0x%lx %10d %.3e %7s %s\n",
                   (unsigned long)data[i].id, data[i].count, (double)data[i].cost,
                   data[i].primary ? "y" : "n", mode) < 0)
            return -1;
    }
    return fflush(stdout);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Использование: %s <файл-а> <файл-б> <результат>\n", argv[0]);
        return EXIT_FAILURE;
    }

    StatData *left = NULL;
    StatData *right = NULL;
    StatData *joined = NULL;
    size_t left_count = 0;
    size_t right_count = 0;
    size_t joined_count = 0;

    if (LoadDump(argv[1], &left, &left_count) != 0) {
        perror(argv[1]);
        goto fail;
    }
    if (LoadDump(argv[2], &right, &right_count) != 0) {
        perror(argv[2]);
        goto fail;
    }
    if (JoinDump(left, left_count, right, right_count,
                 &joined, &joined_count) != 0) {
        perror("Ошибка объединения данных");
        goto fail;
    }

    SortDump(joined, joined_count);
    errno = 0;
    if (print_records(joined, joined_count) != 0) {
        if (errno == 0)
            errno = EIO;
        perror("Ошибка вывода");
        goto fail;
    }
    if (StoreDump(argv[3], joined, joined_count) != 0) {
        perror(argv[3]);
        goto fail;
    }

    free(left);
    free(right);
    free(joined);
    return EXIT_SUCCESS;

fail:
    free(left);
    free(right);
    free(joined);
    return EXIT_FAILURE;
}
