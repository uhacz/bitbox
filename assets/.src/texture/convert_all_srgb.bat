
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

for %%i in (%in_dir%\*.jpg,%in_dir%\*.tga,%in_dir%\*.png) do (
	set ff=%%i
	texconv.exe -srgb -f R8G8B8A8_UNORM_SRGB %%i -o %out_dir%
)
pause
endlocal