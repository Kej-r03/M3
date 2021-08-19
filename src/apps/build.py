dirs = [
    'allocator',
    'asciiplay',
    'bench',
    'coreutils',
    'cppnettests',
    'disktest',
    'dosattack',
    'evilcompute',
    'faulter',
    'filterchain',
    'hello',
    'jpegtest',
    'libctest',
    'msgchan',
    'netechoserver',
    'noop',
    'parchksum',
    'ping',
    'plasma',
    'queue',
    'rusthello',
    'rustnettests',
    'ruststandalone',
    'rustunittests',
    'shell',
    'standalone',
    'timertest',
    'unittests',
]

def build(gen, env):
    for d in dirs:
        env.sub_build(gen, d)
