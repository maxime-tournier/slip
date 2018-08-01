
project('slap', 'cpp',
		default_options : ['cpp_std=c++11',
						   'b_sanitize=address'])

compiler = meson.get_compiler('cpp')
readline = compiler.find_library('readline', required: true)


executable('slap',
		   'main.cpp',
		   'sexpr.cpp',
		   'symbol.cpp',
		   'ast.cpp',
		   'eval.cpp',
		   'tool.cpp',
		   dependencies: [readline])

