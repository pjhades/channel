#!/usr/bin/env python3

import sys
import ninja_syntax

def glibc_since(maj, min):
    import re, subprocess
    s = subprocess.check_output('ldd --version', shell=True).decode()
    m = re.search(r'ldd \(GNU libc\) (\d+).(\d+).*', s)
    return int(m[1]) >= maj and int(m[2]) >= min

if __name__ == '__main__':
    if not sys.platform.startswith('linux'):
        sys.exit("This software only supports Linux")

    w = ninja_syntax.Writer(open('build.ninja', 'w'))

    cflags = ['-std=c11', '-c', '-g', '-Wall', '-Werror', '-Isrc']
    if glibc_since(2, 19):
        cflags.append('-D_DEFAULT_SOURCE')
    else:
        cflags.append('-D_BSD_SOURCE')

    w.variable('cflags', cflags)
    w.variable('ldflags', ['-lpthread', '-latomic'])
    w.variable('cc', 'gcc')
    w.newline()

    w.rule('cc', depfile='$out.d', deps='gcc', command='$cc -MMD -MF $out.d $cflags -o $out $in')
    w.newline()

    w.rule('link', command='$cc -o $out $in $ldflags')
    w.newline()

    w.build('src/mutex.o', 'cc', 'src/mutex.c')
    w.build('src/chan.o', 'cc', 'src/chan.c')
    w.build('test/test_chan.o', 'cc', 'test/test_chan.c')
    w.build('test/test_chan', 'link', ['test/test_chan.o', 'src/chan.o', 'src/mutex.o'])
    w.newline()

    w.default('test/test_chan')

    w.close()
