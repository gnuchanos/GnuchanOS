@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

SET OPAMBAT=%LOCALAPPDATA%\Programs\opam\opam.bat
IF EXIST "%OPAMBAT%" CALL "%OPAMBAT%"
SET PATH=%LOCALAPPDATA%\Programs\ocaml\bin;%PATH%

cd /d "%~dp0"

echo === Compiling GCL-SH Compiler ===

REM Step 1: Compile AST module first (needed for menhir --infer)
ocamlc -c -I src src/ast.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

REM Step 2: Generate parser with menhir (use -I to find ast.cmi)
menhir -I src --base src/parser src/parser.mly --infer 2>&1
IF %ERRORLEVEL% NEQ 0 (
    echo Menhir infer failed. The parser.mly needs %%type declarations.
    echo Trying with the original build method...
    goto :use_dune
)
goto :manual_build

:manual_build
echo Menhir succeeded. Continuing with manual build.

REM Step 3: Generate lexer
ocamllex src/lexer.mll -o src/lexer.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

REM Step 4: Compile all modules
ocamlc -c -I src src/parser.mli 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ocamlc -c -I src src/parser.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ocamlc -c -I src src/lexer.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ocamlc -c -I src src/error.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ocamlc -c -I src src/typechecker.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ocamlc -c -I src src/codegen.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ocamlc -c -I src src/main.ml 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

REM Step 5: Link
ocamlc unix.cma -o gclc.exe ast.cmo parser.cmo lexer.cmo error.cmo typechecker.cmo codegen.cmo main.cmo 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo === Build Successful! ===
echo Binary: gclc.exe
exit /b 0

:use_dune
echo Menhir infer failed. Using dune build instead.
echo First delete old _build, then let dune handle everything.
echo.
echo Hit any key to continue with dune build...
pause >nul

REM Enable menhir in dune-project
echo (lang dune 3.0) > dune-project
echo (using menhir 2.0) >> dune-project
echo (name gcl_sh) >> dune-project

REM Restore dune file with menhir support
echo (executable > src/dune
echo  (name main) >> src/dune
echo  (libraries unix) >> src/dune
echo  (ocamlopt_flags -warn-error -a) >> src/dune
echo  (menhir (modules parser) (infer true)) >> src/dune
echo  (ocamllex lexer) >> src/dune
echo ) >> src/dune

REM Remove old generated files to avoid conflict
del /Q src\parser.ml 2>nul
del /Q src\parser.mli 2>nul
del /Q src\lexer.ml 2>nul
del /Q src\lexer.mli 2>nul

dune build 2>&1
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo === Dune Build Successful! ===
echo Binary: _build/default/src/main.exe
exit /b 0
