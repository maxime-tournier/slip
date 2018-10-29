# -*- mode: lua -*-

project('slip', 'cpp',
        default_options : ['cpp_std=c++14'])

compiler = meson.get_compiler('cpp')
readline = compiler.find_library('readline', required: true)

lib_path = join_paths(meson.source_root(), 'lib')

cpp_args = ['-DSLIP_PATH="@0@"'.format(lib_path)]

if get_option('buildtype') == 'release'
   cpp_args += ['-DNDEBUG']
endif

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
           'builtins.cpp',
           'infer.cpp',
           'unify.cpp',
		   'gc.cpp',
           dependencies: [readline],
           cpp_args : cpp_args)


