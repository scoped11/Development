call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
cl /nologo /c /kernel /GS- /W3 /D_AMD64_ /DAMD64 /DWIN64 /D_WIN64 /D_WIN32_WINNT=0x0A00 ^
  /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\km" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt" ^
  /Fo"C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bin\obj\VmLabDrv.obj" "C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\driver\VmLabDrv.c"
if errorlevel 1 exit /b 1
link /nologo /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /ENTRY:DriverEntry /OUT:"C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bin\VmLabDrv.sys" ^
  "C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bin\obj\VmLabDrv.obj" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\km\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" ^
  ntoskrnl.lib hal.lib BufferOverflowFastFailK.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt" ^
  "C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\driver\VmLabClient.cpp" /Fe"C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bin\VmLabClient.exe" /link ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" kernel32.lib user32.lib
if errorlevel 1 exit /b 1
