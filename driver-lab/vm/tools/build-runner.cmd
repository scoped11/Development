call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /D_WIN32_WINNT=0x0A00 ^
  /I"C:\Users\baile\OneDrive\Desktop\Development\byovd\include" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared" ^
  "C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\tools\LabAcRunner.cpp" /Fe"C:\Users\baile\OneDrive\Desktop\Development\driver-lab\vm\bin\LabAcRunner.exe" /link /MANIFESTUAC:level='requireAdministrator' ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" kernel32.lib user32.lib advapi32.lib shlwapi.lib
if errorlevel 1 exit /b 1
