project('csp_pipeline_config', 'c')

csp_dep = dependency('csp', fallback : ['csp', 'csp_dep'])

csp_pipeline_src = files([
	'src/pipeline_server.c',
])

csp_pipeline_config_inc = include_directories('include')

slash_dep = []
if get_option('slash') == true
	slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)
	if slash_dep.found()
		csp_pipeline_src += files([
			'src/pipeline_slash.c',	
		])
	endif
endif

csp_pipeline_config_lib = static_library('csp_pipeline_config',
	sources: [csp_pipeline_src],
	include_directories : csp_pipeline_config_inc,
	dependencies : [csp_dep, slash_dep],
	install : false
)

csp_pipeline_config_dep = declare_dependency(include_directories : csp_pipeline_config_inc, link_with : csp_pipeline_config_lib)