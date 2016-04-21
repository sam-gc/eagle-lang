import sys

with open('rc.egl', 'r') as _file:
    for line in _file:
        sys.stdout.write('"' + line.rstrip('\n') + '\\n"\n')
