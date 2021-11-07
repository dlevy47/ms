import os
import re
import subprocess

INCLUDES = [
        'third-party.windows\\',
        'C:\\VulkanSDK\\1.2.189.2\\Include\\',
        ]

LIBS = [
        'third-party.windows\\zlib.lib',
        'third-party.windows\\glfw3_mt.lib',
        'C:\\VulkanSDK\\1.2.189.2\\Lib\\vulkan-1.lib',
        'user32.lib',
        'gdi32.lib',
        'shell32.lib',
        ]

def compile(source, additional_includes=[]):
    """Compile the c++ file source, manipulating the path name to generate a unique object name."""
    object = 'build\\' + source.replace('\\', '-').replace(',', '-') + '.obj'
    p = subprocess.run([
        'cl.exe',
        '/Fo' + object,
        '/c',
        '/std:c++20',
        '/Z7',
        '/EHsc',
        '/Ilib\\',
        '/Ithird-party\\'] + list('/I' + i for i in INCLUDES + additional_includes) + [
        source])
    return object

def link(binary_name, objects):
    p = subprocess.run([
        'cl.exe',
        '/Febuild\\' + binary_name + '.exe',
        '/std:c++20',
        '/fsanitize=address',
        '/Z7',
        '/EHsc'] + objects + [
        '/link',
        '/NODEFAULTLIB:MSVCRT'] + LIBS)

lib_objects = []
for dir in [
        'lib',
        'third-party',
        ]:
    for dirpath, _, files in os.walk(dir):
        for file in files:
            if ((not file.endswith('.c') or file.endswith('.posix.c')) and
                    (not file.endswith('.cc') or file.endswith('.posix.cc'))):
                continue

            lib_objects.append(compile(os.path.join(dirpath, file)))

for ent in os.scandir('bin'):
    # At this level, each source file is considered a separate binary.
    if ent.is_file() and ent.name.endswith('.cc'):
        # First, compile this binary's object.
        binary_object = compile(ent.path)

        # Then, link this binary.
        base = re.sub(r'\.cc?$', '', ent.name)
        link(base, [binary_object] + lib_objects)
    elif ent.is_dir():
        # Each directory is considered a separate binary.
        binary_name = ent.name

        additional_includes = [ent.path]
        binary_objects = []
        for dirpath, _, files in os.walk(ent.path):
            for file in files:
                if ((not file.endswith('.c') or file.endswith('.posix.c')) and
                        (not file.endswith('.cc') or file.endswith('.posix.cc'))):
                    continue

                binary_objects.append(compile(os.path.join(dirpath, file), additional_includes))

        link(binary_name, binary_objects + lib_objects)
