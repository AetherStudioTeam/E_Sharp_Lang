#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stddef.h>

typedef enum {
    ES_TASK_RESULT_EXECUTED = 0,
    ES_TASK_RESULT_UP_TO_DATE,
    ES_TASK_RESULT_SKIPPED,
    ES_TASK_RESULT_FAILED
} EsTaskResult;

typedef struct {
    int executed;
    int up_to_date;
    int skipped;
    int failed;
} EsTaskStats;

typedef struct {
    EsTaskStats stats;
    double total_duration;
    int failed;
} EsBuildSummary;

void es_task_report(const char* stage, const char* file, EsTaskResult result, double duration, EsTaskStats* stats);
void es_build_summary_reset(void);
void es_build_summary_accumulate(const EsTaskStats* stats, double duration, int failed);
void es_print_build_summary(void);
void es_build_summary_set_duration(double duration);
void es_build_summary_set_failed(int failed);
EsTaskStats* es_get_global_task_stats(void);

#endif 