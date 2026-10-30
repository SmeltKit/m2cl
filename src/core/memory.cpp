// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "memory.h"
#include "java.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

namespace mc {
QString memorySize(qint64 mib){return mib%1024==0?QString::number(mib/1024)+" GiB":QString::number(mib/1024.,'f',2)+" GiB";}
MemorySnapshot systemMemory(){
    MemorySnapshot s;constexpr qint64 MiB=1024*1024;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};status.dwLength=sizeof(status);
    if(GlobalMemoryStatusEx(&status)){s.totalMiB=qint64(status.ullTotalPhys/MiB);s.availableMiB=qint64(status.ullAvailPhys/MiB);s.source="Windows GlobalMemoryStatusEx";}
#elif defined(Q_OS_MACOS)
    uint64_t total=0;size_t length=sizeof(total);vm_size_t pageSize=0;vm_statistics64_data_t stats{};mach_msg_type_number_t count=HOST_VM_INFO64_COUNT;
    const auto host=mach_host_self();
    const bool ok=sysctlbyname("hw.memsize",&total,&length,nullptr,0)==0&&host_page_size(host,&pageSize)==KERN_SUCCESS&&host_statistics64(host,HOST_VM_INFO64,reinterpret_cast<host_info64_t>(&stats),&count)==KERN_SUCCESS;
    mach_port_deallocate(mach_task_self(),host);
    if(ok){
        s.totalMiB=qint64(total/MiB);
        // An advisory estimate, not Apple's per-process limit (unavailable on macOS).
        // free_count already includes speculative pages. Do not add them twice or count compressed RAM as free.
        s.availableMiB=qint64((uint64_t(stats.free_count)+uint64_t(stats.inactive_count))*pageSize/MiB);
        s.source=QCoreApplication::translate("M2CL", "macOS 空闲与非活跃页面估计（不含压缩内存）");s.estimated=true;
    }
#elif defined(Q_OS_LINUX)
    QFile file("/proc/meminfo");
    if(file.open(QIODevice::ReadOnly)){
        const auto text=QString::fromLatin1(file.readAll());
        auto field=[&](const QString &key){const auto match=QRegularExpression("(?m)^"+key+":\\s+(\\d+)\\s+kB").match(text);return match.hasMatch()?match.captured(1).toLongLong()/1024:qint64(-1);};
        s.totalMiB=field("MemTotal");s.availableMiB=field("MemAvailable");s.source="Linux MemAvailable";
    }
#endif
    if(s.totalMiB<=0||s.availableMiB<0||s.source.isEmpty())s.error=QCoreApplication::translate("M2CL", "未能读取系统可用内存，可重新检测或手动指定");
    else s.availableMiB=qMin(s.availableMiB,s.totalMiB);
    return s;
}
MemoryContext memoryContext(const QString &version,const QString &directory,const QString &loader,const QString &architecture){
    MemoryContext c;c.gameVersion=version;c.gameDirectory=directory;c.loader=loader;c.javaArchitecture=architecture;
    if(loader=="fabric"||loader=="quilt"||loader=="forge"||loader=="neoforge"){
        const QDir mods(QDir(directory).filePath("mods"));
        if(mods.exists()&&!QFileInfo(mods.path()).isReadable())c.note=QCoreApplication::translate("M2CL", "无法读取模组目录，暂按基础需求估计");
        else for(const auto &file:mods.entryInfoList(QDir::Files|QDir::NoSymLinks)){
            const auto suffix=file.suffix().toLower();if(suffix=="jar"||suffix=="zip"||suffix=="litemod")++c.enabledModFiles;
        }
    }
    return c;
}
MemoryRecommendation recommendMemory(const MemorySnapshot &s,const MemoryContext &c){
    MemoryRecommendation r;
    if(!s.error.isEmpty()){r.error=s.error;return r;}
    if(s.totalMiB<=0||s.availableMiB<0||s.availableMiB>s.totalMiB){r.error=QCoreApplication::translate("M2CL", "系统内存统计无效，请重新检测或手动指定");return r;}
    const auto legacy=QRegularExpression("^1\\.(\\d+)(?:\\.|$)").match(c.gameVersion);
    r.targetMiB=legacy.hasMatch()&&legacy.captured(1).toInt()<18?2048:3072;
    QString demand=QCoreApplication::translate("M2CL", "原版基础估计");
    if(c.enabledModFiles>0){r.targetMiB=c.enabledModFiles<=50?4096:c.enabledModFiles<=150?6144:8192;demand=QCoreApplication::translate("M2CL", "%1 个启用的外部模组文件，按规模粗略估计").arg(c.enabledModFiles);}
    if(c.packSuggestedMiB>0){r.targetMiB=c.packSuggestedMiB;demand=QCoreApplication::translate("M2CL", "整合包建议");}
    r.reserveMiB=int(qBound<qint64>(qint64(1024),s.totalMiB/8,qint64(2048)));
    qint64 budget=qMin<qint64>(16384,qMin(s.totalMiB/2,qMax<qint64>(0,s.availableMiB-r.reserveMiB)));
    const bool java32=normalizeArchitecture(c.javaArchitecture)=="x86";if(java32)budget=qMin<qint64>(budget,768);
    r.budgetMiB=int(budget/256*256);r.maximumMiB=qMin(r.targetMiB,r.budgetMiB)/256*256;
    r.basis=QCoreApplication::translate("M2CL", "系统总内存：%1\n当前可用%2：%3\n额外预留：%4\n需求目标：%5（%6）\n自动上限：总内存的一半，最多 16 GiB\n统计来源：%7")
        .arg(memorySize(s.totalMiB),s.estimated?QCoreApplication::translate("M2CL", "估计"):QString(),memorySize(s.availableMiB),memorySize(r.reserveMiB),memorySize(r.targetMiB),demand,s.source);
    if(java32)r.basis+=QCoreApplication::translate("M2CL", "\n32 位 Java：自动分配最多 768 MiB");
    else if(c.javaArchitecture.isEmpty())r.basis+=QCoreApplication::translate("M2CL", "\nJava 位数尚未确认，启动前会按实际运行时重算");
    if(s.estimated)r.basis+=QCoreApplication::translate("M2CL", "\nmacOS 非活跃页面不保证全部可回收，此值仅为保守分配的参考。");
    if(!c.note.isEmpty())r.basis+="\n"+c.note;
    if(r.maximumMiB<512)r.error=QCoreApplication::translate("M2CL", "当前可用内存不足以给出至少 512 MiB 的建议，请关闭其他程序后重试或手动指定");
    else if(c.packSuggestedMiB>r.maximumMiB)r.error=QCoreApplication::translate("M2CL", "当前预算低于整合包建议的 %1，请释放内存或手动处理").arg(memorySize(c.packSuggestedMiB));
    else if(r.maximumMiB<r.targetMiB)r.warning=QCoreApplication::translate("M2CL", "受%1限制，建议暂降至 %2，可能低于此实例实际需求").arg(java32?QCoreApplication::translate("M2CL", "32 位 Java 或可用内存"):QCoreApplication::translate("M2CL", "当前可用内存"),memorySize(r.maximumMiB));
    return r;
}
}
