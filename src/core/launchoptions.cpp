// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "launchoptions.h"

namespace mc {
QStringList splitLaunchArguments(const QString &text,QString *error){
    if(error)error->clear();QStringList result;QString argument;QChar quote;bool started=false;
    for(int i=0;i<text.size();++i){
        const auto c=text.at(i);
        if(c==QChar::Null){if(error)*error=QCoreApplication::translate("M2CL", "参数包含无效字符");return {};}
        if(c=='\\'&&i+1<text.size()&&quote!='\''&&(text.at(i+1)=='"'||(text.at(i+1)=='\''&&quote.isNull()))){argument+=text.at(++i);started=true;continue;}
        if(!quote.isNull()){if(c==quote)quote={};else argument+=c;started=true;}
        else if(c=='"'||c=='\''){quote=c;started=true;}
        else if(c.isSpace()){if(started){result.append(argument);argument.clear();started=false;}}
        else {argument+=c;started=true;}
    }
    if(!quote.isNull()){if(error)*error=QCoreApplication::translate("M2CL", "参数中的英文引号没有闭合");return {};}
    if(started)result.append(argument);return result;
}
QString validateLaunchOptions(const LaunchOptions &o){
    if(o.maxMemoryMiB<512||o.maxMemoryMiB>32768)return QCoreApplication::translate("M2CL", "最大内存须为 512–32768 MiB");
    if(o.minMemoryMiB<0||o.minMemoryMiB>32768||(o.minMemoryMiB>0&&o.minMemoryMiB<128)||(!o.autoMemory&&o.minMemoryMiB>o.maxMemoryMiB))return QCoreApplication::translate("M2CL", "初始内存须为 0（自动），或 128 MiB 至最大内存之间");
    if(o.width<320||o.width>16384||o.height<240||o.height>16384)return QCoreApplication::translate("M2CL", "窗口宽度须为 320–16384，高度须为 240–16384");
    if(o.jvmArguments.size()>32768||o.gameArguments.size()>32768)return QCoreApplication::translate("M2CL", "附加参数过长，请控制在 32768 个字符以内");
    QString error;const auto jvm=splitLaunchArguments(o.jvmArguments,&error);if(!error.isEmpty())return QCoreApplication::translate("M2CL", "JVM 参数：")+error;
    const auto game=splitLaunchArguments(o.gameArguments,&error);if(!error.isEmpty())return QCoreApplication::translate("M2CL", "游戏参数：")+error;
    const QStringList managedJvm{"-jar","-cp","-classpath","--class-path","-m","--module","--"};
    const QStringList valueOptions{"--add-opens","--add-exports","--add-reads","--patch-module","--module-path","-p","--upgrade-module-path","--limit-modules","--enable-native-access"};
    bool expectsValue=false;
    for(const auto &arg:jvm){
        if(expectsValue){if(arg.isEmpty()||arg.startsWith('-'))return QCoreApplication::translate("M2CL", "JVM 参数缺少选项值");expectsValue=false;continue;}
        if(arg.startsWith("-Xmx")||arg.startsWith("-Xms")||arg.startsWith("-XX:MaxHeapSize=")||arg.startsWith("-XX:InitialHeapSize="))return QCoreApplication::translate("M2CL", "请通过内存设置调整 -Xmx / -Xms，不要在 JVM 参数中重复填写");
        const auto key=arg.section('=',0,0);if(managedJvm.contains(key)||arg.startsWith("-Djava.class.path="))return QCoreApplication::translate("M2CL", "%1 由启动器生成，请勿在附加 JVM 参数中重复填写").arg(key);
        if(arg.startsWith('@'))return QCoreApplication::translate("M2CL", "暂不支持 JVM @参数文件，请直接填写参数内容");
        if(!arg.startsWith('-'))return QCoreApplication::translate("M2CL", "附加 JVM 参数必须是选项，不能填写游戏主类：%1").arg(arg);
        expectsValue=valueOptions.contains(arg);
    }
    if(expectsValue)return QCoreApplication::translate("M2CL", "JVM 参数缺少选项值");
    const QStringList managedGame{"--width","--height","--fullscreen","--gameDir","--assetsDir","--assetIndex","--username","--uuid","--accessToken","--session","--userType","--userProperties","--version","--versionType","--clientId","--xuid"};
    for(const auto &arg:game)if(managedGame.contains(arg.section('=',0,0)))return QCoreApplication::translate("M2CL", "%1 已由实例设置或启动器管理，请勿重复填写").arg(arg.section('=',0,0));
    return {};
}
}
