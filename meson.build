project('csp_ippc', 'c')

csp_dep = dependency('csp', fallback : ['csp', 'csp_dep'])
param_dep = dependency('param', fallback : ['param', 'param_dep'])
proto_c_dep = dependency('libprotobuf-c', fallback: ['protobuf-c', 'proto_c_dep'])
jxl_dep = dependency('libjxl', version: '>= 0.7.0')
brotli_dep = dependency('libbrotlienc')

csp_ippc_src = files([
	'src/protobuf/pipeline_config.pb-c.c',
	'src/protobuf/module_config.pb-c.c',
	'src/protobuf/metadata.pb-c.c',
])

csp_ippc_inc = include_directories('src/include', 'src/include/protobuf')

slash_dep = []
if get_option('slash') == true
	slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)
	if slash_dep.found()
		csp_ippc_src += files([
			'src/pipeline_slash.c',	
		])
	endif
endif

csp_ippc_lib = static_library('csp_ippc',
	sources: [csp_ippc_src],
	include_directories : csp_ippc_inc,
	dependencies : [csp_dep, slash_dep, param_dep, proto_c_dep, jxl_dep, brotli_dep],
	install : false
)

csp_ippc_dep = declare_dependency(include_directories : csp_ippc_inc, link_with : csp_ippc_lib)