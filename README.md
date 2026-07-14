<div align="center">
  <img src="./logo.png" alt="Gnuchanos" width="120" />
</div>

<div align="center"><strong>Gnuchanos</strong> — kişisel GNU/Linux denemesi (rolling).</div>

<div align="center">GnuChanGUI eski PySimpleGUI tabanlı lib → <a href="https://www.github.com/gnuchanos/gnuchangui" target="_blank">GitHub</a></div>

<hr>

maybe one day but not today

<h1>Language</h1>
<h2>: Finish</h2>
--: gcl -version, -v
--: gcl -luarun path/file.lua -dll path/lua55.dll or -so path/liblua55.so
    binding and embed:--> gcl_raygui.dll / .so
    binding and embed:--> gcl_raylib.dll / .so
    binding and embed:--> lua55.dll / .so
--: gcl pyrun path/file.py -dll path/python314.dll or path/python314.so
    fetch_python_embeddable.py path # this is download moduler python gcl need


<h2>: NOT Finish</h2>
--: gcl -> interactive shell

# path link extra options
--: gcl -linclude path/folder
--: gcl -llib path/folder
--: gcl -lextend path/folder

# debug
--: gcl -lexer file.gcsf
--: gcl -parser file.gcsf
--: gcl -ast file.gcsf
--: gcl -ir file.gcsf
--: gcl -codegen file.gcsf

# export or run
--: gcl -run path/file.gcsf
--: gcl -all_flags file.gcsf -o path/output

# extra export
--: gcl -wasm raylib
--: gcl -wasm binding
--: gcl -wasm export

--: gcl -debug run
--: gcl -version
--: gcl -help