#ifndef CONSOLE_UTILS_H
#define CONSOLE_UTILS_H


#define ES_COL_RESET "\033[0m"
#define ES_COL_BOLD "\033[1m"
#define ES_COL_GREEN "\033[32m"
#define ES_COL_RED "\033[31m"
#define ES_COL_CYAN "\033[36m"
#define ES_COL_YELLOW "\033[33m"
#define ES_COL_GRAY "\033[90m"
#define ES_COL_MAGENTA "\033[35m"
#define ES_COL_BLUE "\033[34m"


#define ANSI_CURSOR_UP "\033[F"
#define ANSI_CLEAR_LINE "\033[K"
#define ANSI_SAVE_CURSOR "\033[s"
#define ANSI_RESTORE_CURSOR "\033[u"

int es_console_supports_color(void);
void es_console_set_color_enabled(int enabled);
const char* es_color(const char* code);

#endif 