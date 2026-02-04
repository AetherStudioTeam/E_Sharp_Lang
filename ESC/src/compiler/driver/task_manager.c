#include "task_manager.h"
#include "console_utils.h"
#include "../../core/utils/es_common.h"
#include "../../core/utils/output_cache.h"
#include <stdio.h>
#include <string.h>

static EsBuildSummary g_build_summary = {0};
static int g_current_task_line_active = 0;

void es_task_report(const char* stage, const char* file, EsTaskResult result, double duration, EsTaskStats* stats) {
    if (!stage || !stats) return;

    const char* status = "";
    const char* color = ES_COL_RESET;
    switch (result) {
        case ES_TASK_RESULT_EXECUTED:
            stats->executed++;
            break;
        case ES_TASK_RESULT_UP_TO_DATE:
            stats->up_to_date++;
            status = " UP-TO-DATE";
            color = ES_COL_YELLOW;
            break;
        case ES_TASK_RESULT_SKIPPED:
            stats->skipped++;
            status = " SKIPPED";
            color = ES_COL_GRAY;
            break;
        case ES_TASK_RESULT_FAILED:
            stats->executed++;
            stats->failed++;
            break;
    }

    if (g_current_task_line_active) {
        es_printf(ANSI_CURSOR_UP ANSI_CLEAR_LINE);
    }

    if (result == ES_TASK_RESULT_EXECUTED || result == ES_TASK_RESULT_UP_TO_DATE || result == ES_TASK_RESULT_SKIPPED) {
        es_printf("%s>%s %s", es_color(ES_COL_CYAN), es_color(ES_COL_RESET), stage);
        if (file && file[0] != '\0') {
            es_printf(" %s%s%s", es_color(ES_COL_GRAY), file, es_color(ES_COL_RESET));
        }
        if (status[0] != '\0') {
            es_printf(" %s%s%s", es_color(color), status, es_color(ES_COL_RESET));
        }
        if (duration >= 0.0 && duration > 0.01) {
            es_printf(" %s%.1fs%s", es_color(ES_COL_GRAY), duration, es_color(ES_COL_RESET));
        }
        es_printf("\n");
        g_current_task_line_active = 1;
    } else if (result == ES_TASK_RESULT_FAILED) {
        es_printf("%s>%s %s", es_color(ES_COL_CYAN), es_color(ES_COL_RESET), stage);
        if (file && file[0] != '\0') {
            es_printf(" %s%s%s", es_color(ES_COL_GRAY), file, es_color(ES_COL_RESET));
        }
        es_printf(" %sFAILED%s\n", es_color(ES_COL_RED), es_color(ES_COL_RESET));
        g_current_task_line_active = 0;
    }

    if (result == ES_TASK_RESULT_FAILED) {
        es_output_cache_flush();
        g_current_task_line_active = 0;
    }
}

static void es_show_idle_status(void) {
    if (g_current_task_line_active) {
        es_printf(ANSI_CURSOR_UP ANSI_CLEAR_LINE);
        es_printf("%s>%s %sIDLE%s\n", es_color(ES_COL_CYAN), es_color(ES_COL_RESET), es_color(ES_COL_GRAY), es_color(ES_COL_RESET));
        g_current_task_line_active = 0;
    }
}

void es_build_summary_reset(void) {
    memset(&g_build_summary, 0, sizeof(g_build_summary));
    g_current_task_line_active = 0;
}

void es_build_summary_accumulate(const EsTaskStats* stats, double duration, int failed) {
    if (!stats) return;
    g_build_summary.stats.executed += stats->executed;
    g_build_summary.stats.up_to_date += stats->up_to_date;
    g_build_summary.stats.skipped += stats->skipped;
    g_build_summary.stats.failed += stats->failed;
    g_build_summary.total_duration += duration;
    if (failed) {
        g_build_summary.failed = 1;
    }
}

void es_print_build_summary(void) {
    int total_tasks = g_build_summary.stats.executed + g_build_summary.stats.up_to_date + g_build_summary.stats.skipped;
    if (total_tasks <= 0) {
        return;
    }

    es_show_idle_status();
    es_output_cache_flush();

    const char* status_text = g_build_summary.failed ? "FAILED" : "SUCCESSFUL";
    const char* status_color = g_build_summary.failed ? ES_COL_RED : ES_COL_GREEN;

    es_printf("\n%s%sBUILD %s%s in %.1fs\n",
           es_color(ES_COL_BOLD), es_color(status_color), status_text, es_color(ES_COL_RESET),
           g_build_summary.total_duration);

    int executed = g_build_summary.stats.executed;
    int up_to_date = g_build_summary.stats.up_to_date;
    int skipped = g_build_summary.stats.skipped;
    int failed = g_build_summary.stats.failed;

    if (total_tasks > 0) {
        es_printf("%s%d tasks: %s", es_color(ES_COL_GRAY), total_tasks, es_color(ES_COL_RESET));

        int first = 1;
        if (executed > 0) {
            es_printf("%s%d executed%s", es_color(ES_COL_GREEN), executed, es_color(ES_COL_RESET));
            first = 0;
        }
        if (up_to_date > 0) {
            if (!first) es_printf(", ");
            es_printf("%s%d up-to-date%s", es_color(ES_COL_YELLOW), up_to_date, es_color(ES_COL_RESET));
            first = 0;
        }
        if (skipped > 0) {
            if (!first) es_printf(", ");
            es_printf("%s%d skipped%s", es_color(ES_COL_GRAY), skipped, es_color(ES_COL_RESET));
            first = 0;
        }
        if (failed > 0) {
            if (!first) es_printf(", ");
            es_printf("%s%d failed%s", es_color(ES_COL_RED), failed, es_color(ES_COL_RESET));
            first = 0;
        }
        es_printf("\n");
    }
}

void es_build_summary_set_duration(double duration) {
    g_build_summary.total_duration = duration;
}

void es_build_summary_set_failed(int failed) {
    g_build_summary.failed = failed;
}

EsTaskStats* es_get_global_task_stats(void) {
    return &g_build_summary.stats;
}
