@echo off
if not exist build mkdir build
gcc main.c editor_win.c lexerf.c parserf.c semanticf.c codegeneratorf.c hashmap/hashmapoperators.c -o build\unn.exe -Wall -Wextra -luser32 -lgdi32 -lcomdlg32
exit /b %errorlevel%
