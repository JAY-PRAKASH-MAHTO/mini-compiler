@echo off
if not exist build mkdir build
gcc main.c diagnostics.c editor_win.c lexerf.c parserf.c semanticf.c codegeneratorf.c -o build\bhasacore.exe -Wall -Wextra -std=c11 -luser32 -lgdi32 -lcomdlg32
exit /b %errorlevel%
