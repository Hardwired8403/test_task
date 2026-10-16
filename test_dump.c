#define _POSIX_C_SOURCE 200809L

#include "statdata.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const StatData case_1_in_a[] = {
    {.id = 90889, .count = 13, .cost = 3.567f, .primary = 0, .mode = 3},
    {.id = 90089, .count = 1, .cost = 88.90f, .primary = 1, .mode = 0},
};
static const StatData case_1_in_b[] = {
    {.id = 90089, .count = 13, .cost = 0.011f, .primary = 0, .mode = 2},
    {.id = 90189, .count = 1000, .cost = 1.00003f, .primary = 1, .mode = 2},
};
static const StatData case_1_expected[] = {
    {.id = 90189, .count = 1000, .cost = 1.00003f, .primary = 1, .mode = 2},
    {.id = 90889, .count = 13, .cost = 3.567f, .primary = 0, .mode = 3},
    {.id = 90089, .count = 14, .cost = 88.90f + 0.011f, .primary = 0, .mode = 2},
};
static const StatData case_2_in_a[] = {
    {.id = -7, .count = 4, .cost = 9.0f, .primary = 1, .mode = 1},
    {.id = -7, .count = -1, .cost = -2.0f, .primary = 1, .mode = 7},
    {.id = 42, .count = 2, .cost = 4.0f, .primary = 0, .mode = 3},
};
static const StatData case_2_in_b[] = {
    {.id = -7, .count = 3, .cost = -1.0f, .primary = 0, .mode = 4},
    {.id = 100, .count = 8, .cost = 0.0f, .primary = 1, .mode = 0},
    {.id = 42, .count = -2, .cost = -4.0f, .primary = 1, .mode = 5},
};
static const StatData case_2_expected[] = {
    {.id = 42, .count = 0, .cost = 0.0f, .primary = 0, .mode = 5},
    {.id = 100, .count = 8, .cost = 0.0f, .primary = 1, .mode = 0},
    {.id = -7, .count = 6, .cost = 6.0f, .primary = 0, .mode = 7},
};
static const StatData case_3_in_a[] = {
    {.id = 5, .count = 1, .cost = 5.0f, .primary = 1, .mode = 1},
    {.id = 1, .count = 1, .cost = 1.0f, .primary = 1, .mode = 1},
    {.id = 9, .count = 1, .cost = 9.0f, .primary = 1, .mode = 1},
    {.id = 3, .count = 1, .cost = 3.0f, .primary = 1, .mode = 1},
    {.id = 7, .count = 1, .cost = 7.0f, .primary = 1, .mode = 1},
    {.id = 2, .count = 1, .cost = 2.0f, .primary = 1, .mode = 1},
    {.id = 8, .count = 1, .cost = 8.0f, .primary = 1, .mode = 1},
    {.id = 4, .count = 1, .cost = 4.0f, .primary = 1, .mode = 1},
    {.id = 6, .count = 1, .cost = 6.0f, .primary = 1, .mode = 1},
    {.id = 0, .count = 1, .cost = 0.0f, .primary = 1, .mode = 1},
    {.id = 10, .count = 1, .cost = 10.0f, .primary = 1, .mode = 1},
};
static const StatData case_3_expected[] = {
    {.id = 0, .count = 1, .cost = 0.0f, .primary = 1, .mode = 1},
    {.id = 1, .count = 1, .cost = 1.0f, .primary = 1, .mode = 1},
    {.id = 2, .count = 1, .cost = 2.0f, .primary = 1, .mode = 1},
    {.id = 3, .count = 1, .cost = 3.0f, .primary = 1, .mode = 1},
    {.id = 4, .count = 1, .cost = 4.0f, .primary = 1, .mode = 1},
    {.id = 5, .count = 1, .cost = 5.0f, .primary = 1, .mode = 1},
    {.id = 6, .count = 1, .cost = 6.0f, .primary = 1, .mode = 1},
    {.id = 7, .count = 1, .cost = 7.0f, .primary = 1, .mode = 1},
    {.id = 8, .count = 1, .cost = 8.0f, .primary = 1, .mode = 1},
    {.id = 9, .count = 1, .cost = 9.0f, .primary = 1, .mode = 1},
    {.id = 10, .count = 1, .cost = 10.0f, .primary = 1, .mode = 1},
};
static const StatData *const empty = NULL;

typedef struct TestCase {
    const char *name;
    const StatData *left;
    size_t left_count;
    const StatData *right;
    size_t right_count;
    const StatData *expected;
    size_t expected_count;
    const char *expected_output;
} TestCase;

static const TestCase cases[] = {
    {
        "Пример объединения и формата", case_1_in_a, 2, case_1_in_b, 2,
        case_1_expected, 3,
        "0x1604d       1000 1.000e+00       y 10\n"
        "0x16309         13 3.567e+00       n 11\n"
        "0x15fe9         14 8.891e+01       n 10\n",
    },
    {"Дубликаты и отрицательные значения", case_2_in_a, 3, case_2_in_b, 3,
     case_2_expected, 3, NULL},
    {"Сортировка и вывод первых десяти записей", case_3_in_a, 11, empty, 0,
     case_3_expected, 11, NULL},
    {"Пустые входные файлы", empty, 0, empty, 0, empty, 0, NULL},
};

static int fail(const char *message)
{
    fprintf(stderr, "ОШИБКА: %s\n", message);
    return 1;
}

static int run_program(const char *program, const char *input_a,
                       const char *input_b, const char *output,
                       const char *log_path)
{
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        int log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (log_fd < 0 || dup2(log_fd, STDOUT_FILENO) < 0)
            _exit(126);
        close(log_fd);
        execl(program, program, input_a, input_b, output, (char *)NULL);
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int output_contains(const char *path, const char *expected)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return 0;
    char output[4096];
    size_t length = fread(output, 1, sizeof(output) - 1, file);
    int ok = !ferror(file);
    fclose(file);
    output[length] = '\0';
    return ok && strstr(output, expected) != NULL;
}

static int output_row_count(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return -1;
    char line[256];
    if (fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, "Идентификатор      Количество Стоимость Основной Режим\n") != 0) {
        fclose(file);
        return -1;
    }
    int rows = 0;
    while (fgets(line, sizeof(line), file) != NULL)
        ++rows;
    int ok = !ferror(file);
    fclose(file);
    return ok ? rows : -1;
}

static int compare_result(const TestCase *test, const char *path)
{
    StatData *actual = NULL;
    size_t actual_count = 0;
    if (LoadDump(path, &actual, &actual_count) != 0)
        return 0;
    int equal = actual_count == test->expected_count;
    for (size_t i = 0; equal && i < actual_count; ++i) {
        equal = actual[i].id == test->expected[i].id &&
                actual[i].count == test->expected[i].count &&
                actual[i].cost == test->expected[i].cost &&
                actual[i].primary == test->expected[i].primary &&
                actual[i].mode == test->expected[i].mode;
    }
    free(actual);
    return equal;
}

static int test_truncated_dump(const char *path)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    static const unsigned char truncated[] = {'S', 'T', 'D', 'A', 'T', 'A', '0', '1'};
    int ok = fwrite(truncated, 1, sizeof(truncated), file) == sizeof(truncated);
    if (fclose(file) != 0)
        ok = 0;
    StatData *records = NULL;
    size_t count = 0;
    if (ok && LoadDump(path, &records, &count) == 0)
        ok = 0;
    free(records);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return fail("использование: test_dump <путь-к-утилите>");

    char directory[] = "/tmp/statdata-test-XXXXXX";
    if (mkdtemp(directory) == NULL)
        return fail("не удалось создать временный каталог");

    struct timespec started;
    struct timespec finished;
    if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) {
        rmdir(directory);
        return fail("ошибка clock_gettime");
    }

    int failures = 0;
    size_t passed = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char input_a[PATH_MAX], input_b[PATH_MAX], output[PATH_MAX], log[PATH_MAX];
        snprintf(input_a, sizeof(input_a), "%s/%zu-a.bin", directory, i);
        snprintf(input_b, sizeof(input_b), "%s/%zu-b.bin", directory, i);
        snprintf(output, sizeof(output), "%s/%zu-out.bin", directory, i);
        snprintf(log, sizeof(log), "%s/%zu.log", directory, i);

        const TestCase *test = &cases[i];
        int ok = StoreDump(input_a, test->left, test->left_count) == 0 &&
                 StoreDump(input_b, test->right, test->right_count) == 0 &&
                 run_program(argv[1], input_a, input_b, output, log) == 0 &&
                 compare_result(test, output) &&
                 output_row_count(log) ==
                     (int)(test->expected_count < 10 ? test->expected_count : 10);
        if (ok && test->expected_output != NULL)
            ok = output_contains(log, test->expected_output);
        if (ok) {
            printf("ПРОЙДЕН: %s\n", test->name);
            ++passed;
        } else {
            fprintf(stderr, "ОШИБКА: тест «%s» не пройден\n", test->name);
            ++failures;
        }
        unlink(input_a);
        unlink(input_b);
        unlink(output);
        unlink(log);
    }

    char malformed[PATH_MAX];
    snprintf(malformed, sizeof(malformed), "%s/malformed.bin", directory);
    if (test_truncated_dump(malformed)) {
        puts("ПРОЙДЕН: отклонение усечённого файла");
        ++passed;
    } else {
        fprintf(stderr, "ОШИБКА: не отклонён усечённый файл\n");
        ++failures;
    }
    unlink(malformed);

    StatData overflow_input[] = {
        {.id = 1, .count = INT_MAX, .cost = 1.0f, .primary = 1, .mode = 0},
        {.id = 1, .count = 1, .cost = 1.0f, .primary = 1, .mode = 0},
    };
    StatData *joined = NULL;
    size_t joined_count = 0;
    if (JoinDump(overflow_input, 2, NULL, 0, &joined, &joined_count) != 0) {
        puts("ПРОЙДЕН: обнаружение переполнения count");
        ++passed;
    } else {
        fprintf(stderr, "ОШИБКА: не обнаружено переполнение count\n");
        ++failures;
    }
    free(joined);

    if (clock_gettime(CLOCK_MONOTONIC, &finished) != 0) {
        rmdir(directory);
        return fail("ошибка clock_gettime");
    }
    double elapsed_ms =
        (double)(finished.tv_sec - started.tv_sec) * 1000.0 +
        (double)(finished.tv_nsec - started.tv_nsec) / 1000000.0;
    printf("Тестов пройдено: %zu, ошибок: %d; время выполнения: %.3f мс\n",
           passed, failures, elapsed_ms);
    rmdir(directory);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
