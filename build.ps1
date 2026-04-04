$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Path "build" -Force | Out-Null

gcc main.c editor_win.c lexerf.c parserf.c semanticf.c codegeneratorf.c hashmap/hashmapoperators.c -o build/unn.exe -Wall -Wextra -luser32 -lgdi32 -lcomdlg32

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
