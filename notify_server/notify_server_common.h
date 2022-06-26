#ifndef __NOTIFY_SERVER_COMMON_H__
#define __NOTIFY_SERVER_COMMON_H__

#ifdef __GNUC__
# define UNUSED(x) UNUSED_##x __attribute__((__unused__))
#else
# define UNUSED(x) UNUSED_##x
#endif

#define NOTIFY_LOG_ERROR(fmt, ...) printf("{notify_server} [ERROR] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define NOTIFY_LOG_WARN(fmt, ...) printf("{notify_server} [WARN] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define NOTIFY_LOG_INFO(fmt, ...) printf("{notify_server} [INFO] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define NOTIFY_LOG_DEBUG(fmt, ...) printf("{notify_server} [DEBUG] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)

#endif // !__NOTIFY_SERVER_COMMON_H__