REM This bat-file is used temporary for preparing the Android generated files for R3
r3-make.exe make-headers.r | more
r3-make.exe make-boot.r 0.13.1  | more
r3-make.exe make-os-ext.r | more
r3-make.exe make-reb-lib.r | more
r3-make.exe saphir-init.r saphir-view | more
r3-make.exe core-ext.r | more
r3-make.exe view-ext.r | more
