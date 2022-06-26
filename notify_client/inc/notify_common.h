#ifndef __NOTIFY_COMMON_H__
#define __NOTIFY_COMMON_H__

#define MODULE_EVENT_SIZE 100

typedef enum {
    MODULE_BROADCAST = 0, /* 广播 */
    MODULE_NOTIFY_SERVER, /* notify server */
    MODULE_TEST1,
    MODULE_TEST2,
    MODULE_ID_MAX, /* 模块最大数，在下面新增模块无效 */
} NotifyModuleId;

/* event 与 modid 并无直接关系，此处区分event是为了便于管理 */
typedef enum {
    /* 注册notify */
    NOTIFY_REGISTER = MODULE_NOTIFY_SERVER * MODULE_EVENT_SIZE,
    NOTIFY_UNREGISTER, /* 注销notify */
    SERVER_WRITE_SEND_TEST, /* notify server读写测试 */

    MODULE_TEST1_EVENT1 = MODULE_TEST1 * MODULE_EVENT_SIZE,
    MODULE_TEST1_EVENT2,
    MODULE_TEST1_EVENT3,
    MODULE_TEST1_EVENT4,

    MODULE_TEST2_EVENT1 = MODULE_TEST2 * MODULE_EVENT_SIZE,
    MODULE_TEST2_EVENT2,
    MODULE_TEST2_EVENT3,
    MODULE_TEST2_EVENT4,
} NotifyEvent;

#endif // !__NOTIFY_CLIENT_COMMON_H__
