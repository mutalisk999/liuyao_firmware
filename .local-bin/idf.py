#!/bin/sh
# Local helper: run ESP-IDF idf.py under Git Bash on Windows.
# The sandbox re-injects MSYSTEM into every spawned process, which trips
# ESP-IDF's MSys check; the in-process pop in the python wrapper defeats it.
export IDF_PATH='C:\Users\mutalisk\esp\esp-idf-v5.5.3'
export IDF_TOOLS_PATH='C:\Users\mutalisk\.espressif'
PYENV=/c/Users/mutalisk/.espressif/python_env/idf5.5_py3.11_env/Scripts
TOOLS=/c/Users/mutalisk/.espressif/tools
export PATH="$PYENV:$TOOLS/cmake/3.30.2/bin:$TOOLS/ninja/1.12.1:$TOOLS/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin:$TOOLS/ccache/4.12.1:$PATH"
exec "$PYENV/python.exe" -c "import os, sys, runpy; sys.path.insert(0, r'C:\Users\mutalisk\esp\esp-idf-v5.5.3\tools'); os.environ.pop('MSYSTEM', None); sys.argv = ['idf.py'] + sys.argv[1:]; sys.exit(runpy.run_path(r'C:\Users\mutalisk\esp\esp-idf-v5.5.3\tools\idf.py', run_name='__main__'))" "$@"
