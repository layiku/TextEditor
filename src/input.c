/*
 * input.c — 输入处理实现
 * 轻量封装平台 plat_poll_event，未来可在此层做事件过滤/重映射
 */
#include "input.h"
#include "platform/platform.h"

void input_init(void) {
    /* 平台初始化已在 display_init → plat_init 中完成，此处留扩展接口 */
}

void input_exit(void) {
    /* 同上 */
}

int input_wait_event(InputEvent *ev, int timeout_ms) {
    return plat_poll_event(ev, timeout_ms);
}

int input_poll(InputEvent *ev) {
    return plat_poll_event(ev, 0);
}
