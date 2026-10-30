// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "credentialvault.h"
#include <QProcess>
#include <QStandardPaths>
#ifdef Q_OS_MACOS
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
#endif
namespace mc {
QString SystemCredentialVault::name() const {
#ifdef Q_OS_MACOS
    return QCoreApplication::translate("M2CL", "macOS 钥匙串");
#elif defined(Q_OS_WIN)
    return QCoreApplication::translate("M2CL", "Windows 凭据管理器");
#else
    return QCoreApplication::translate("M2CL", "系统密钥环（Secret Service）");
#endif
}
bool SystemCredentialVault::available() const {
#if defined(Q_OS_MACOS)||defined(Q_OS_WIN)
    return true;
#else
    return !QStandardPaths::findExecutable("secret-tool").isEmpty();
#endif
}
CredentialResult SystemCredentialVault::read(const QString &key){return operation(key,{},0);}
QString SystemCredentialVault::write(const QString &key,const QString &secret){if(secret.isEmpty()||secret.toUtf8().size()>32768)return QCoreApplication::translate("M2CL", "刷新凭据长度无效");return operation(key,secret,1).error;}
QString SystemCredentialVault::remove(const QString &key){return operation(key,{},2).error;}
CredentialResult SystemCredentialVault::operation(const QString &key,const QString &secret,int kind){
    CredentialResult result;if(key.isEmpty()){result.error=QCoreApplication::translate("M2CL", "凭据标识无效");return result;}
#ifdef Q_OS_MACOS
    auto string=[](const QString &s){const auto b=s.toUtf8();return CFStringCreateWithBytes(nullptr,reinterpret_cast<const UInt8 *>(b.constData()),b.size(),kCFStringEncodingUTF8,false);};
    auto service=string(m_service),account=string(key);auto query=CFDictionaryCreateMutable(nullptr,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query,kSecClass,kSecClassGenericPassword);CFDictionarySetValue(query,kSecAttrService,service);CFDictionarySetValue(query,kSecAttrAccount,account);CFDictionarySetValue(query,kSecUseAuthenticationUI,kSecUseAuthenticationUIFail);
    OSStatus status=errSecSuccess;
    if(kind==0){CFDictionarySetValue(query,kSecReturnData,kCFBooleanTrue);CFDictionarySetValue(query,kSecMatchLimit,kSecMatchLimitOne);CFTypeRef value=nullptr;status=SecItemCopyMatching(query,&value);if(status==errSecSuccess&&value&&CFGetTypeID(value)==CFDataGetTypeID())result.value=QString::fromUtf8(reinterpret_cast<const char *>(CFDataGetBytePtr(static_cast<CFDataRef>(value))),CFDataGetLength(static_cast<CFDataRef>(value)));if(value)CFRelease(value);}
    else if(kind==2)status=SecItemDelete(query);
    else {const auto bytes=secret.toUtf8();auto data=CFDataCreate(nullptr,reinterpret_cast<const UInt8 *>(bytes.constData()),bytes.size());auto update=CFDictionaryCreateMutable(nullptr,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);CFDictionarySetValue(update,kSecValueData,data);status=SecItemUpdate(query,update);if(status==errSecItemNotFound){CFDictionarySetValue(query,kSecValueData,data);status=SecItemAdd(query,nullptr);}CFRelease(update);CFRelease(data);}
    CFRelease(query);CFRelease(service);CFRelease(account);if(status!=errSecSuccess&&!(kind==2&&status==errSecItemNotFound))result.error=status==errSecItemNotFound?QCoreApplication::translate("M2CL", "未找到保存的登录凭据，请重新登录"):QCoreApplication::translate("M2CL", "无法访问 macOS 钥匙串（%1），可仅在本次会话中登录").arg(status);
#elif defined(Q_OS_WIN)
    const auto target=(m_service+"/"+key).toStdWString();DWORD code=ERROR_SUCCESS;
    if(kind==0){PCREDENTIALW credential=nullptr;if(CredReadW(target.c_str(),CRED_TYPE_GENERIC,0,&credential)){result.value=QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob),credential->CredentialBlobSize);CredFree(credential);}else code=GetLastError();}
    else if(kind==2){if(!CredDeleteW(target.c_str(),CRED_TYPE_GENERIC,0))code=GetLastError();if(code==ERROR_NOT_FOUND)code=ERROR_SUCCESS;}
    else {auto bytes=secret.toUtf8();if(bytes.size()>CRED_MAX_CREDENTIAL_BLOB_SIZE){result.error=QCoreApplication::translate("M2CL", "凭据超过 Windows 密钥库大小限制，可仅在本次会话中登录");return result;}CREDENTIALW credential{};credential.Type=CRED_TYPE_GENERIC;credential.TargetName=const_cast<wchar_t *>(target.c_str());credential.UserName=const_cast<wchar_t *>(L"M2CL");credential.CredentialBlobSize=DWORD(bytes.size());credential.CredentialBlob=reinterpret_cast<LPBYTE>(bytes.data());credential.Persist=CRED_PERSIST_LOCAL_MACHINE;if(!CredWriteW(&credential,0))code=GetLastError();}
    if(code!=ERROR_SUCCESS)result.error=QCoreApplication::translate("M2CL", "无法访问 Windows 凭据管理器（%1），请重新登录或仅在本次会话中登录").arg(code);
#else
    const auto tool=QStandardPaths::findExecutable("secret-tool");if(tool.isEmpty()){result.error=QCoreApplication::translate("M2CL", "系统缺少 secret-tool，无法记住登录；可安装 libsecret 工具，或仅在本次会话中登录");return result;}
    QStringList args;args<<(kind==0?"lookup":kind==1?"store":"clear");if(kind==1)args<<"--label=Mercury MC Launcher";args<<"application"<<m_service<<"account"<<key;
    QProcess p;p.start(tool,args);if(!p.waitForStarted(3000)){result.error=QCoreApplication::translate("M2CL", "无法启动系统密钥环工具");return result;}if(kind==1)p.write(secret.toUtf8());p.closeWriteChannel();if(!p.waitForFinished(10000)){p.kill();p.waitForFinished(2000);result.error=QCoreApplication::translate("M2CL", "系统密钥环访问超时，可解锁后重试或仅在本次会话中登录");return result;}
    if(p.exitStatus()!=QProcess::NormalExit||(p.exitCode()!=0&&!(kind==2&&p.exitCode()==1)))result.error=QCoreApplication::translate("M2CL", "无法访问系统密钥环，请检查是否已解锁，或仅在本次会话中登录");
    else if(kind==0){auto bytes=p.readAllStandardOutput();if(bytes.endsWith('\n'))bytes.chop(1);if(bytes.size()>32768)result.error=QCoreApplication::translate("M2CL", "系统密钥环返回了无效数据");else result.value=QString::fromUtf8(bytes);}
#endif
    if(kind==0&&result.error.isEmpty()&&result.value.isEmpty())result.error=QCoreApplication::translate("M2CL", "未找到保存的登录凭据，请重新登录");return result;
}
}
