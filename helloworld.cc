#include <stdio.h>
#include <uv.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int64_t idle_count = 0;
int64_t timer_count = 0;
int64_t check_count = 0;
int64_t prepare_count = 0;

void idle_phage_func(uv_idle_t* handle) {
    idle_count++;
    printf(ANSI_COLOR_GREEN "idle\n" ANSI_COLOR_RESET);

    if (idle_count >= 10e6)
        uv_idle_stop(handle);
}

void timer_phage_func(uv_timer_t* handle) {
    timer_count++;
    printf(ANSI_COLOR_BLUE "timer\n" ANSI_COLOR_RESET);

    if (timer_count >= 10e6)
        uv_timer_stop(handle);
}

void check_phage_func(uv_check_t* handle) {
    check_count++;
    printf(ANSI_COLOR_RED "check\n" ANSI_COLOR_RESET);

    if (check_count >= 10e6)
        uv_check_stop(handle);
}

void prepare_phage_func(uv_prepare_t* handle) {
    prepare_count++;
    printf(ANSI_COLOR_RED "prepare\n" ANSI_COLOR_RESET);

    if (prepare_count >= 10e6)
        uv_prepare_stop(handle);
}

int main() {
    uv_loop_t *loop = uv_default_loop();
    // uv_loop_t *loop1 = uv_default_loop();

    uv_idle_t idler;
    uv_timer_t timer;
    uv_check_t check;
    uv_prepare_t prepare;
    
    // printf("loop pointer address singletone check\n");
    // printf("%p\n", &loop);
    // printf("%p\n", &loop1);

    uv_idle_init(loop, &idler); // idle phage 초기화
    uv_idle_start(&idler, idle_phage_func);
    
    uv_timer_init(loop, &timer); // timer phage 초기화
    uv_timer_start(&timer, timer_phage_func, 0, 2);

    uv_check_init(loop, &check); // check phage 초기화
    uv_check_start(&check, check_phage_func);

    uv_prepare_init(loop, &prepare); // prepare phage 초기화
    uv_prepare_start(&prepare, prepare_phage_func);

    // uv_run(loop, UV_RUN_DEFAULT);  // 이벤트 루프를 계속 동작시킨다.
    uv_run(loop, UV_RUN_ONCE);  // 이벤트 루프를 한 번만 동작시킨다.
    uv_loop_close(loop);
    return 0;
}