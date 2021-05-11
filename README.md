* build & run

```sh
$ gcc -o helloworld helloworld.cc -luv

$ gcc -o test test.cc -luv

$ ./helloworld

$ ./test
```

# 디자인 

libuv는 Node.js 용으로 작성된 크로스 플랫폼 지원 라이브러리입니다. 이벤트 기반 비동기 I/O 모델을 중심으로 설계되었습니다.

libuv는 다양한 I/O 폴링 메커니즘에 대한 단순한 추상화 이상의 기능을 제공합니다. 핸들(handle), 스트림(stream)은 소켓 및 기타 엔티티에 대한 높은 수준의 추상화를 제공합니다. 크로스 플랫폼 파일 I/O 및 스레딩 기등도 제공합니다.

다음은 libuv를 구성하는 다양한 부분과 관련 하위 시스템을 보여주는 다이어그램입니다.

![](http://docs.libuv.org/en/v1.x/_images/architecture.png)

### Handles and Requests

libuv는 사용자에게 이벤트 루프와 함께 작업 할 두 가지 추상화 (핸들 및 요청)를 제공합니다.

핸들은 활성 상태에서 특정 작업을 수행 할 수있는 수명이 긴(long-lived) 개체를 나타냅니다. 

* 준비핸들(prepare handle)은 활성화 될 때 루프 반복마다 한 번씩 콜백을 호출합니다.

* 새 연결이 있을때 마다 호출되는 연결 콜백을 가져오는 TCP 서버 핸들입니다.

요청(reqeust)은 일반적으로 단기 작업(short-lived)을 나타냅니다. 이러한 작업은 핸들을 통해 수행 할 수 있습니다. 쓰기 요청은 핸들에 데이터를 쓰는 데 사용됩니다. 또는 독립형 : getaddrinfo 요청은 루프에서 직접 실행되는 핸들이 필요하지 않습니다.

```cpp
/* Handle types. */
typedef struct uv_loop_s uv_loop_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_dir_s uv_dir_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_udp_s uv_udp_t;
typedef struct uv_pipe_s uv_pipe_t;
typedef struct uv_tty_s uv_tty_t;
typedef struct uv_poll_s uv_poll_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_async_s uv_async_t;
typedef struct uv_process_s uv_process_t;
typedef struct uv_fs_event_s uv_fs_event_t;
typedef struct uv_fs_poll_s uv_fs_poll_t;
typedef struct uv_signal_s uv_signal_t;

/* Request types. */
typedef struct uv_req_s uv_req_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;
typedef struct uv_getnameinfo_s uv_getnameinfo_t;
typedef struct uv_shutdown_s uv_shutdown_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;
typedef struct uv_work_s uv_work_t;
```

### I/O Loop

I/O 루프는 libuv의 핵심 요소입니다. 모든 I/O 작업에 대한 내용을 설정하며 단일 스레드에 연결됨을 의미합니다. 각각 다른 스레드에서 실행되는 한 여러 이벤트 루프를 실행할 수 있습니다. libuv 이벤트 루프 (또는 루프 또는 핸들과 관련된 다른 API)는 달리 명시되지 않는 한 thread-safe 하지 않습니다.

```cpp
void reader(void *n)
{
    int num = *(int *)n;
    int i;
    for (i = 0; i < 20; i++) {
        uv_rwlock_rdlock(&numlock);
        printf("Reader %d: acquired lock\n", num);
        printf("Reader %d: shared num = %d\n", num, shared_num);
        uv_rwlock_rdunlock(&numlock);
        printf("Reader %d: released lock\n", num);
    }
    uv_barrier_wait(&blocker);
}

void writer(void *n)
{
    int num = *(int *)n;
    int i;
    for (i = 0; i < 20; i++) {
        uv_rwlock_wrlock(&numlock);
        printf("Writer %d: acquired lock\n", num);
        shared_num++;
        printf("Writer %d: incremented shared num = %d\n", num, shared_num);
        uv_rwlock_wrunlock(&numlock);
        printf("Writer %d: released lock\n", num);
    }
    uv_barrier_wait(&blocker);
}
```

`uv_rwlock_wrlock()` 명시하여 thread-safe 할 수 있습니다.

이벤트 루프는 다소 일반적인 단일 스레드 비동기 I/O 접근 방식을 따릅니다. 모든 (네트워크) I/O는 주어진 플랫폼에서 사용할 수있는 최상의 메커니즘을 사용하여 폴링되는 비 차단 소켓에서 수행됩니다. Linux의 `epoll`, OSX의 `kqueue` 및 기타 BSD, SunOS의 이벤트 포트 및 Windows의 `IOCP`. 루프 반복의 일환으로 루프는 폴러(poller)에 추가된 소켓에서 I/O 활동을 기다리는 것을 차단하고 소켓 상태(읽기 가능, 쓰기 가능 중단)를 나타내는 콜백이 발생하므로 핸들이 원하는 I/O 작업을 읽고, 쓰거나 수행할 수 있습니다.

이벤트 루프의 작동 방식을 더 잘 이해하기 위해 다음 다이어그램은 루프 반복의 모든 단계를 보여줍니다.

![](http://docs.libuv.org/en/v1.x/_images/loop_iteration.png)


1. 'now'의 루프 컨셉이 업데이트되었습니다. 이벤트 루프는 시간 관련 시스템 호출의 수를 줄이기 위해 이벤트 루프 틱이 시작될 때 현재 시간을 캐시합니다.

2. 루프가 살아 있으면 반복이 시작되고 그렇지 않으면 루프가 즉시 종료됩니다. 그렇다면 루프는 언제 살아있는 것으로 간주됩니까? 루프에 활성 및 참조 핸들, 활성 요청 또는 닫기 핸들이있는 경우 활성 상태로 간주됩니다.

3. due timer를 실행합니다. 이제 루프의 개념이 이전에 예약된 모든 활성 타이머(setTimeout, setInterval)는 콜백이 호출됩니다.

4. 보류중(pending)인 콜백을 호출합니다. 모든 I/O 콜백은 대부분 I/O 폴링 직후 호출합니다. 그러나 이러한 콜백 호출을 다음 루프 반복을 위해 지연(deffer)하는 경우가 있습니다. 이전 반복이 I/O 콜백을 연기(deffer) 한 경우이 시점에서 실행합니다.

5. idle handle 콜백을 호출합니다. 불행한 이름에도 불구하고 idle handle은 활성 상태 인 경우 모든 루프 반복에서 실행합니다.

6. prepare handle 콜백을 호출합니다. 준비 핸들은 루프가 I/O를 위해 차단되기 직전에 콜백을 호출합니다.

7. 폴링 제한 시간을 계산합니다. I/O를 차단하기 전에 루프는 차단해야하는 시간을 계산합니다. 제한시간 계산 규칙은 다음과 같습니다.

```
  * 루프가 `UV_RUN_NOWAIT` 플래그로 실행 된 경우 시간 제한은 0입니다.

  * 루프가 중지 될 예정이면 (`uv_stop()` 호출 됨) 시간 제한은 0입니다.

  * 활성 핸들 또는 요청이없는 경우 제한 시간은 0입니다.

  * 활성 idle 핸들이있는 경우 제한 시간은 0입니다.

  * 닫히도록 보류중(pending)인 핸들이있는 경우 시간 제한은 0입니다.

  * 위의 경우 중 어느 것도 일치하지 않으면 가장 가까운 타이머의 시간 초과가 적용되거나 활성 타이머가없는 경우 무한대입니다.
```

8. I/O를위한 루프 블록. 이 시점에서 루프는 이전 단계에서 계산 된 기간 동안 I/O를 위해 차단됩니다. 읽기 또는 쓰기 작업을 위해 주어진 파일 디스크립터(descriptor)를 모니터링하던 모든 I/O 관련 핸들은이 시점에서 콜백을받습니다.

9. check handle 콜백을 호출합니다. check handle은 I/O에 대해 루프가 차단 된 직후에 콜백을 호출합니다. check handle은 본질적으로 prepare handle의 대응 부분입니다.

10. close 콜백을 호출합니다. `uv_close()` 호출하여 핸들을 닫으면 close 콜백을 호출합니다.

11. 순방향 진행을 의미하므로 `UV_RUN_ONCE`로 루프를 실행 한 경우 특별한 경우입니다. I/O를 차단 한 후 I/O 콜백이 실행되지 않았을 수 있지만 시간이 지나서 예정된 타이머가있을 수 있으며 해당 타이머가 콜백을 호출합니다.

12. 반복을 종료합니다. 루프가 `UV_RUN_NOWAIT` 또는 `UV_RUN_ONCE` 모드로 실행 된 경우 반복이 종료되고 `uv_run()`이 반환됩니다. 루프가 `UV_RUN_DEFAULT`로 실행 된 경우 아직 살아 있으면 처음부터 계속되고 그렇지 않으면 종료됩니다.

13. **중요**: libuv는 스레드 풀을 사용하여 비동기 파일 I/O 작업을 가능하게하지만 네트워크 I/O는 항상 단일 스레드, 각 루프의 스레드에서 수행됩니다.

```
폴링 메커니즘은 다르지만 libuv는 Unix 시스템과 Windows에서 실행 모델을 일관되게 만듭니다.
```

다음 코드는 libuv에서 각각 페이즈를 위한 정의인터페이스 입니다.

```cpp
enum uv_poll_event {
  UV_READABLE = 1,
  UV_WRITABLE = 2,
  UV_DISCONNECT = 4,
  UV_PRIORITIZED = 8
};
UV_EXTERN int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd);
UV_EXTERN int uv_poll_init_socket(uv_loop_t* loop,
                                  uv_poll_t* handle,
                                  uv_os_sock_t socket);
UV_EXTERN int uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb cb);
UV_EXTERN int uv_poll_stop(uv_poll_t* handle);


struct uv_prepare_s {
  UV_HANDLE_FIELDS
  UV_PREPARE_PRIVATE_FIELDS
};

UV_EXTERN int uv_prepare_init(uv_loop_t*, uv_prepare_t* prepare);
UV_EXTERN int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb);
UV_EXTERN int uv_prepare_stop(uv_prepare_t* prepare);


struct uv_check_s {
  UV_HANDLE_FIELDS
  UV_CHECK_PRIVATE_FIELDS
};

UV_EXTERN int uv_check_init(uv_loop_t*, uv_check_t* check);
UV_EXTERN int uv_check_start(uv_check_t* check, uv_check_cb cb);
UV_EXTERN int uv_check_stop(uv_check_t* check);


struct uv_idle_s {
  UV_HANDLE_FIELDS
  UV_IDLE_PRIVATE_FIELDS
};

UV_EXTERN int uv_idle_init(uv_loop_t*, uv_idle_t* idle);
UV_EXTERN int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb);
UV_EXTERN int uv_idle_stop(uv_idle_t* idle);


struct uv_async_s {
  UV_HANDLE_FIELDS
  UV_ASYNC_PRIVATE_FIELDS
};

UV_EXTERN int uv_async_init(uv_loop_t*,
                            uv_async_t* async,
                            uv_async_cb async_cb);
UV_EXTERN int uv_async_send(uv_async_t* async);


/*
 * uv_timer_t is a subclass of uv_handle_t.
 *
 * Used to get woken up at a specified time in the future.
 */
struct uv_timer_s {
  UV_HANDLE_FIELDS
  UV_TIMER_PRIVATE_FIELDS
};

UV_EXTERN int uv_timer_init(uv_loop_t*, uv_timer_t* handle);
UV_EXTERN int uv_timer_start(uv_timer_t* handle,
                             uv_timer_cb cb,
                             uint64_t timeout,
                             uint64_t repeat);
UV_EXTERN int uv_timer_stop(uv_timer_t* handle);
UV_EXTERN int uv_timer_again(uv_timer_t* handle);
UV_EXTERN void uv_timer_set_repeat(uv_timer_t* handle, uint64_t repeat);
UV_EXTERN uint64_t uv_timer_get_repeat(const uv_timer_t* handle);
UV_EXTERN uint64_t uv_timer_get_due_in(const uv_timer_t* handle);
```

### File I/O

File I/O는 네트워크 I/O와 달리 플랫폼(Window, Mac, Linux) 별 의지할 수 있는 요소가 없기 때문에 쓰레드 풀에서 블록킹(blocking) 파일 I/O를 작업합니다.

각각의 환경에 따른 파일 I/O는 [여기서](https://blog.libtorrent.org/2012/10/asynchronous-disk-io/) 확인하세요

libuv는 현재 모든 루프가 작업을 대기열에 넣을 수있는 전역 스레드 풀을 사용합니다. 현재이 풀에서 3 가지 유형의 작업이 실행되고 있습니다.

* 파일 시스템 연산

* DNS functions(getaddrinfo & getnameinfo)

* uv_queue_work()를 통한 사용자 지정 코드
