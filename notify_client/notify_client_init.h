
#ifndef __NOTIFY_CLIENT_INIT_H__
#define __NOTIFY_CLIENT_INIT_H__

int NotifyInit(void);

#define NOTIFY_LOG_ERROR(fmt, ...) printf("{notify_client} [ERROR] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define NOTIFY_LOG_WARN(fmt, ...) printf("{notify_client} [WARN] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define NOTIFY_LOG_INFO(fmt, ...) printf("{notify_client} [INFO] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define NOTIFY_LOG_DEBUG(fmt, ...) printf("{notify_client} [DEBUG] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)

#endif // !__NOTIFY_CLIENT_INIT_H__