# XFY USB Key

用 USB 代替密码登录 Windows。

## 项目结构

```
XFYUsbKey/
├── XFYUsbKey.sln                       # 统一解决方案
├── XFYUsbKey.CredentialProvider/       # C++ COM DLL — 锁屏认证磁贴
│   ├── XFYUsbKey.vcxproj
│   ├── CXFYCredential.h/cpp
│   ├── CXFYProvider.h/cpp
│   ├── Dll.h/cpp
│   └── ...
├── XFYUsbKey.Manager/                  # C# WPF — 密钥管理配置工具
│   ├── XFYUsbKey.Manager.csproj
│   ├── App.xaml/cs
│   ├── MainWindow.xaml/cs
│   └── ...
├── tools/
│   └── deploy.ps1                      # VM 部署脚本
└── logs/                               # DebugView 日志
```

## 开发

打开根目录的 `XFYUsbKey.sln` 即可看到两个项目。

### 部署

VM 内管理员运行：
```
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
```
