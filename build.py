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

sources = []
for dir in [
        'lib',
        'third-party',
        ]:
    for dirpath, _, files in os.walk(dir):
        for file in files:
            if ((not file.endswith('.c') or file.endswith('.posix.c')) and
                    (not file.endswith('.cc') or file.endswith('.posix.cc'))):
                continue

            sources.append(os.path.join(dirpath, file))

objects = {}
for source in sources:
    objects[source] = 'build\\' + source.replace('\\', '-').replace('.', '-') + '.obj'
    print(source, objects[source])

for source, object in objects.items():
    p = subprocess.run([
        'cl.exe',
        '/Fo' + object,
        '/c',
        '/std:c++20',
        '/Z7',
        '/EHsc',
        '/Ilib\\',
        '/Ithird-party\\'] + list('/I' + i for i in INCLUDES) + [
        source])

for dirpath, _, files in os.walk('bin'):
    for file in files:
        if ((not file.endswith('.c') or file.endswith('.posix.c')) and
                (not file.endswith('.cc') or file.endswith('.posix.cc'))):
            continue

        base = re.sub(r'\.cc?$', '', file)
        p = subprocess.run([
            'cl.exe',
            '/Febuild\\' + base + '.exe',
            '/std:c++20',
            # '/fsanitize=address',
            '/Z7',
            '/EHsc',
            '/Ilib\\',
            '/Ithird-party\\'] + list('/I' + i for i in INCLUDES) + [
            os.path.join(dirpath, file)] + list(objects.values()) + [
            '/link',
            '/NODEFAULTLIB:MSVCRT'] + LIBS)
