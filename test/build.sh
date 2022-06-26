#!/bin/bash
MYPATH="/home/lala0903/c_learn/threadpool"

cd ${MYPATH}/notify_client/src
gcc -shared -fPIC -lpthread -I ../inc *.c -o libnotify.so
# 解决连接时找不到库的问题
cp libnotify.so /usr/lib
rm ./libnotify.so
cd -

gcc test_process1.c -lnotify -lpthread -g -I ${MYPATH}/notify_client/inc/ -o test1
gcc test_process2.c -lnotify -lpthread -g -I ${MYPATH}/notify_client/inc/ -o test2
gcc test_thread.c -lnotify -lpthread -g -I ${MYPATH}/notify_client/inc/ -o test_thread

cd ${MYPATH}/notify_server
gcc *.c  -lpthread -g -I ${MYPATH}/notify_client/inc/ -o notify_server
cd -

cp ${MYPATH}/notify_server/notify_server ./
rm ${MYPATH}/notify_server/notify_server
