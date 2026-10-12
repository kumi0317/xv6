// kernel/sysinfo.h
#ifndef SYSINFO_H
#define SYSINFO_H

#include "types.h" // 确保可以使用 uint64

struct sysinfo
{
    uint64 freemem; // 空闲内存字节数
    uint64 nproc;   // 活跃进程数
};

#endif