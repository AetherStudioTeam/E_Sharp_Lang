#include "ir_profile.h"
#include <stdio.h>
#include <string.h>



EsIRProfiler g_ir_profiler = {0};
EsIRMemoryStats g_ir_memory_stats = {0};
EsIRPerformanceCounters g_ir_counters = {0};



void es_ir_profiler_init(void) {
    memset(&g_ir_profiler, 0, sizeof(g_ir_profiler));
}

void es_ir_profiler_reset(void) {
    es_ir_profiler_init();
}

static const char* phase_name(EsProfilePhase phase) {
    switch (phase) {
        case ES_PROFILE_LEXER:    return "Lexer     ";
        case ES_PROFILE_PARSER:   return "Parser    ";
        case ES_PROFILE_SEMANTIC: return "Semantic  ";
        case ES_PROFILE_IR_GEN:   return "IR Gen    ";
        case ES_PROFILE_IR_OPT:   return "IR Opt    ";
        case ES_PROFILE_CODEGEN:  return "Codegen   ";
        case ES_PROFILE_LINKING:  return "Linking   ";
        case ES_PROFILE_TOTAL:    return "TOTAL     ";
        default:                  return "Unknown   ";
    }
}

void es_ir_profiler_print(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║            IR Compilation Profile                ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Phase          Time (ms)    Calls   Avg (ms)     ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    
    double total = 0;
    for (int i = 0; i < ES_PROFILE_PHASE_COUNT; i++) {
        if (g_ir_profiler.times[i] > 0) {
            double avg = g_ir_profiler.counts[i] > 0 
                ? g_ir_profiler.times[i] / g_ir_profiler.counts[i] 
                : 0;
            printf("║ %s  %8.3f    %5d   %8.3f    ║\n",
                   phase_name(i),
                   g_ir_profiler.times[i],
                   g_ir_profiler.counts[i],
                   avg);
            total += g_ir_profiler.times[i];
        }
    }
    
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ %s  %8.3f                        ║\n", 
           phase_name(ES_PROFILE_TOTAL), total);
    printf("╚══════════════════════════════════════════════════╝\n");
}

void es_ir_profile_begin(EsProfilePhase phase) {
    if (phase < 0 || phase >= ES_PROFILE_PHASE_COUNT) return;
    if (g_ir_profiler.active[phase]) return;  
    
    g_ir_profiler.start_times[phase] = clock();
    g_ir_profiler.active[phase] = 1;
    g_ir_profiler.counts[phase]++;
}

void es_ir_profile_end(EsProfilePhase phase) {
    if (phase < 0 || phase >= ES_PROFILE_PHASE_COUNT) return;
    if (!g_ir_profiler.active[phase]) return;  
    
    clock_t end = clock();
    double elapsed = ((double)(end - g_ir_profiler.start_times[phase])) 
                     * 1000.0 / CLOCKS_PER_SEC;
    g_ir_profiler.times[phase] += elapsed;
    g_ir_profiler.active[phase] = 0;
}



void es_ir_memory_track_alloc(size_t size) {
    g_ir_memory_stats.total_allocated += size;
    g_ir_memory_stats.current_used += size;
    g_ir_memory_stats.allocation_count++;
    
    if (g_ir_memory_stats.current_used > g_ir_memory_stats.peak_used) {
        g_ir_memory_stats.peak_used = g_ir_memory_stats.current_used;
    }
}

void es_ir_memory_track_free(size_t size) {
    g_ir_memory_stats.total_freed += size;
    g_ir_memory_stats.current_used -= size;
    g_ir_memory_stats.free_count++;
}

void es_ir_memory_print(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║            IR Memory Statistics                  ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Total Allocated:    %10zu bytes            ║\n", g_ir_memory_stats.total_allocated);
    printf("║ Total Freed:        %10zu bytes            ║\n", g_ir_memory_stats.total_freed);
    printf("║ Current Used:       %10zu bytes            ║\n", g_ir_memory_stats.current_used);
    printf("║ Peak Used:          %10zu bytes            ║\n", g_ir_memory_stats.peak_used);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Allocation Count:   %10zu                 ║\n", g_ir_memory_stats.allocation_count);
    printf("║ Free Count:         %10zu                 ║\n", g_ir_memory_stats.free_count);
    printf("╚══════════════════════════════════════════════════╝\n");
}



void es_ir_counters_reset(void) {
    memset(&g_ir_counters, 0, sizeof(g_ir_counters));
}

void es_ir_counters_print(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         IR Performance Counters                  ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Instructions Generated:  %10d             ║\n", g_ir_counters.inst_count);
    printf("║ Basic Blocks:            %10d             ║\n", g_ir_counters.block_count);
    printf("║ Functions:               %10d             ║\n", g_ir_counters.function_count);
    printf("║ Variables:               %10d             ║\n", g_ir_counters.var_count);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Optimization Passes:     %10d             ║\n", g_ir_counters.opt_passes);
    printf("║ Optimizations Applied:   %10d             ║\n", g_ir_counters.opt_applied);
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║ Arena Allocations:       %10d             ║\n", g_ir_counters.arena_allocs);
    printf("║ Malloc Allocations:      %10d             ║\n", g_ir_counters.malloc_allocs);
    printf("║ Arena Bytes:             %10zu bytes        ║\n", g_ir_counters.arena_bytes);
    printf("╚══════════════════════════════════════════════════╝\n");
}
