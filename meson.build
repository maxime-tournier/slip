
project('slap', 'cpp',
		default_options : ['cpp_std=c++14'])

compiler = meson.get_compiler('cpp')
readline = compiler.find_library('readline', required: true)

executable('slap',
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
		   'core.cpp',
                   'infer.cpp',
                   'unify.cpp',
		   dependencies: [readline])

