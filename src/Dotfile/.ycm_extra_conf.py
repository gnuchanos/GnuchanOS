import os
import ycm_core

flags = [
    '-Wall',
    '-Wextra',
    '-Werror',
    '-std=c11',
    '-std=c++20',
    '-x', 'c++',
    '-I', '.',
    '-I', 'include',
    '-I', '/usr/include',
    '-I', '/usr/local/include',
]

compilation_database_folder = ''

if compilation_database_folder:
    database = ycm_core.CompilationDatabase(compilation_database_folder)
else:
    database = None

def DirectoryOfThisScript():
    return os.path.dirname(os.path.abspath(__file__))

def MakeRelativePathsInFlagsAbsolute(flags, working_directory):
    if not working_directory:
        return list(flags)
    new_flags = []
    make_next_absolute = False
    path_flags = ['-isystem', '-I', '-iquote', '--sysroot=']
    for flag in flags:
        new_flag = flag
        if make_next_absolute:
            make_next_absolute = False
            if not flag.startswith('/'):
                new_flag = os.path.join(working_directory, flag)
        for path_flag in path_flags:
            if flag == path_flag:
                make_next_absolute = True
                break
            if flag.startswith(path_flag):
                path = flag[len(path_flag):]
                if not path.startswith('/'):
                    new_flag = path_flag + os.path.join(working_directory, path)
                break
        new_flags.append(new_flag)
    return new_flags

def Settings(**kwargs):
    language = kwargs.get('language', '')
    if language == 'cfamily':
        if database:
            try:
                filename = kwargs['filename']
                compilation_info = database.GetCompilationInfoForFile(filename)
                final_flags = MakeRelativePathsInFlagsAbsolute(
                    compilation_info.compiler_flags_,
                    compilation_info.compiler_working_dir_)
            except Exception:
                final_flags = MakeRelativePathsInFlagsAbsolute(flags, DirectoryOfThisScript())
        else:
            final_flags = MakeRelativePathsInFlagsAbsolute(flags, DirectoryOfThisScript())
        return {'flags': final_flags}
    
    # Python kısmı boş bırakılıyor:
    if language == 'python':
        return {}

    return {}
