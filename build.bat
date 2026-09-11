@echo off
rem build.bat -- compila roboclone.exe com o MSVC Build Tools instalado.
rem Uso: build.bat  (gera roboclone.exe nesta pasta)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /nologo /EHsc /std:c++17 /O2 /DNOMINMAX /Fe:"%~dp0roboclone.exe" ^
   "%~dp0src\main.cpp" ^
   "%~dp0src\atalho.cpp" ^
   "%~dp0src\janela_alvo.cpp" ^
   "%~dp0src\captura_tela.cpp" ^
   "%~dp0src\config.cpp" ^
   "%~dp0src\calibracao.cpp" ^
   "%~dp0src\entrada.cpp" ^
   "%~dp0src\escrever_texto.cpp" ^
   user32.lib gdi32.lib ole32.lib oleaut32.lib uuid.lib /link /MANIFEST:EMBED
del "%~dp0*.obj" >nul 2>&1
echo.
echo build ok:
echo   calibrar: roboclone.exe calibrar
echo   rodar:    roboclone.exe rodar
