
@echo off
setlocal EnableDelayedExpansion

set prev_dir=%CD%
set in_dir=%CD%
cd ..
cd ..
cd texture
set out_dir=%CD%
set out_dir=%out_dir:\=/%
cd %prev_dir%

for %%i in (%in_dir%\*_cubemap.jpg,%in_dir%\*_cubemap.tga,%in_dir%\*_cubemap.png) do (
	set ff=%%i
	texconv.exe -m 11 -f BC6H_UF16 %%i -o %out_dir%
)
pause
endlocal