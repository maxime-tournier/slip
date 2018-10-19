# -*- mode: lua -*-

project('slap', 'cpp',
        default_options : ['cpp_std=c++14'])

compiler = meson.get_compiler('cpp')
readline = compiler.find_library('readline', required: true)

lib_path = join_paths(meson.source_root(), 'lib')

executable('slip',
           'main.cpp',
           'sexpr.cpp',
           'symbol.cpp',
           'ast.cpp',
           'repr.cpp',
           'eval.cpp',
           'tool.cpp',
           'type.cpp',
           'substitution.cpp',
           'kind.cpp',
           'package.cpp',
           'builtin.cpp',
           'infer.cpp',
           'unify.cpp',
           dependencies: [readline],
           cpp_args : '-DSLIP_PATH="@0@"'.format(lib_path))


