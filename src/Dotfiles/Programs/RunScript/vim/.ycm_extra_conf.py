import os
import ycm_core

flags = [
    '-Wall',
    '-Wextra',
    '-Werror',
    '-std=c11',  # C için C11 standardını kullan
    '-x', 'c',   # C dosyaları için
    '-I', '.',   # Geçerli dizini dahil et
    '-I', '/usr/include',
    '-I', '/usr/local/include',
    '-I', '/usr/include/python3.10',  # Python başlık dosyaları
]

compilation_database_folder = ''

if compilation_database_folder:
    database = ycm_core.CompilationDatabase(compilation_database_folder)
else:
    database = None

def Settings(**kwargs):
    language = kwargs['language']
    if language == 'cfamily':
        return {'flags': flags}
    elif language == 'python':
        return {
            'interpreter_path': '/usr/bin/python3',  # Python yolu
            'sys_path': [
                '/usr/lib/python3.10',
                '/usr/lib/python3.10/site-packages'
            ]
        }
    return {}
