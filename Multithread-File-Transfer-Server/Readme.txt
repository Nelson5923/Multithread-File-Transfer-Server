How to Compile and Test
1. gcc -o myftpserver myftpserver.c -lpthread
2. gcc -o myftpclient myftpclient.c -lpthread
3. ./myftpserver 1234
4. (ctrl + z) then (bg)
5. ./myftpclient 127.0.0.1 1234 list

Problem Unfixed
1. Network Byte Ordering (Communicate between different OS)
2. Get Command
3. Put Command

Sparc is a architecture developed by Oracle
Sparc Server share the same file (cuhk)
nano modify the file and test in sparc.
sizeof(pointer) doest work
need to return instead of exit(0) otherwise server will halt.
reusable port when creating listening socket