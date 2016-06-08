#
# Copyright (c) 2015-2016 Sam Horlbeck Olsen
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Yes, yes, I know this code is horrible... It has lots of duplication
# (woo copy paste!) and it generally could be structured waaaaaay better.
# This was just a hack to move the source over to having string literal
# messages stored in a central location in the code. I have much higher
# standards for the code in the compiler itself. Maybe someday I will
# return to this code.
#
# Say what you will about how ugly it is; writing this code and replacing
# ALL of the die() and warn() string literals in the compiler took just
# over an hour.

import sys
import re

var = ''

def prompt(text):
    sys.stdout.write(text + ' ')
    sys.stdout.flush()
    return raw_input()

externs = []
code = []

prefix = 'msgerr_'
prefixwarn = 'msgwarn_'
fname  = prompt('File:')

fulltext = ''
with open(fname, 'r') as f:
    fulltext = f.read()

maxName = 0

def print_spaces(f, name):
    f.write(' ' * (1 + maxName - len(name)))

instances = re.findall(r'die\(.*(".*")', fulltext)
varnames  = []
ignore = set()
for i in instances:
    print i
    var = prefix + '_'.join(prompt('Err > ').split(' '))
    print ''

    if ':' in var:
        var = var.split(':')[0]
        ignore.add(var)

    if len(var) > maxName:
        maxName = len(var)
    varnames.append(var)

for i in range(len(instances)):
    fulltext = re.sub(r'(die\(.*)".*"', '\\1' + varnames[i], fulltext, 1)

instanceswarn = re.findall(r'warn\(.*(".*")', fulltext)
varnameswarn  = []
for i in instanceswarn:
    print i
    var = prefixwarn + '_'.join(prompt('Warn > ').split(' '))
    print ''

    if ':' in var:
        var = var.split(':')[0]
        ignore.add(var)

    if len(var) > maxName:
        maxName = len(var)
    varnameswarn.append(var)

for i in range(len(instanceswarn)):
    fulltext = re.sub(r'(warn\(.*)".*"', '\\1' + varnameswarn[i], fulltext, 1)

with open(fname, 'w') as f:
    f.write(fulltext)

with open('src/core/messages.h', 'a') as f:
    f.write('// ' + fname + '\n')
    for v in varnames:
        if v in ignore:
            continue
        f.write('extern const char *' + v + ';\n')
    for v in varnameswarn:
        if v in ignore:
            continue
        f.write('extern const char *' + v + ';\n')
    f.write('\n')

with open('src/core/messages.c', 'a') as f:
    f.write('// ' + fname + '\n')
    for i in range(len(instances)):
        if varnames[i] in ignore:
            continue

        f.write('const char *' + varnames[i])
        print_spaces(f, varnames[i])
        f.write('= ' + instances[i] + ';\n')
    for i in range(len(instanceswarn)):
        if varnameswarn[i] in ignore:
            continue

        f.write('const char *' + varnameswarn[i])
        print_spaces(f, varnameswarn[i])
        f.write('= ' + instanceswarn[i] + ';\n')
    f.write('\n')

