#ifndef __NOTIFY_COMMON_H__
#define __NOTIFY_COMMON_H__

#define MODULE_EVENT_SIZE 100 
enum NotifyModuleId {
    MODULE_NOTIFY_SERVER = 0,
    MODULE_TEST1,
    MODULE_TEST2,
    MODULE_ID_MAX,
};

enum NotifyEvent {
    NOTIFY_SERVER_EVENT_START = MODULE_NOTIFY_SERVER * MODULE_EVENT_SIZE,


    MODULE_TEST1_EVENT1 = MODULE_TEST1 * MODULE_EVENT_SIZE,
    MODULE_TEST1_EVENT2,
    MODULE_TEST1_EVENT3,
    MODULE_TEST1_EVENT4,

    MODULE_TEST2_EVENT1 = MODULE_TEST2 * MODULE_EVENT_SIZE,
    MODULE_TEST2_EVENT2,
    MODULE_TEST2_EVENT3,
    MODULE_TEST2_EVENT4,
};

#endif // !__NOTIFY_CLIENT_COMMON_H__