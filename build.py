import os
import subprocess

sources = []
for dir in [
	'lib',
	'third-party',
]:
	for dirpath, _, files in os.walk(dir):
		for file in files:
			if not file.endswith('.c') or file.endswith('.posix.c'):
				continue

			sources.append(os.path.join(dirpath, file))

objects = {}
for source in sources:
	objects[source] = 'build\\' + source.replace('\\', '-').replace('.c', '.obj')
	print(source, objects[source])

for source, object in objects.items():
	p = subprocess.run([
		'cl.exe',
		'/std:c11',
		'/Fo' + object,
		'/c',
		'/Z7',
		'/Ilib\\',
		'/Ithird-party\\',
		'/Ithird-party.windows\\',
		source
		])

for dirpath, _, files in os.walk('bin'):
	for file in files:
		if not file.endswith('.c') or file.endswith('.posix.c'):
			continue

		base = file[:-2]
		p = subprocess.run([
			'cl.exe',
			'/std:c11',
			'/Febuild\\' + base + '.exe',
			'/fsanitize=address',
			'/Zi',
			'/Ilib\\',
			'/Ithird-party\\',
			'/Ithird-party.windows\\',
			os.path.join(dirpath, file)] + list(objects.values()) + [
			'/link',
			'third-party.windows\\zlib.lib',
			])