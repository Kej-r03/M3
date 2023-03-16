import src.tools.ninjagen as ninjagen

def build(gen, env):
    env = env.clone()
    env.remove_flag('CXXFLAGS', '-flto')

    files = env.glob('*.cc')

    # build files manually here to specify the exact file name of the object file. we reference
    # them later in the configure.py to ensure that we use our own memcpy etc. implementation.
    for f in files:
        env.cxx(gen, ninjagen.BuildPath.with_ending(env, f, '.o'), [f])

    # same for the soft-float version
    sfenv = env.clone()
    sfenv.soft_float()
    for f in files:
        sfenv.cxx(gen, ninjagen.BuildPath.with_ending(sfenv, f, '-sf.o'), [f])
