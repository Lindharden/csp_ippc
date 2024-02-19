#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp_autoconfig.h>
#include <csp/csp_hooks.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param.h>
#include <param/param_list.h>

#include "include/csp_pipeline_config/pipeline_slash.h"

static int slash_csp_configure_pipeline(struct slash *slash)
{
    optparse_t * parser = optparse_new("pipeline_conf", "<config-file>");
    optparse_add_help(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing config filename\n");
		return SLASH_EINVAL;
	}

	char * config_filename = slash->argv[argi];

    if (strlen(config_filename) == 0) {
        printf("Config file path cannot be empty\n");
        return SLASH_EINVAL;
    }

    printf("Client: Configuring pipeline using %s\n", config_filename);
    
    return configure_pipeline();
}

int configure_pipeline()
{
	char * name = "pipeline_run";
	int node = 162;
	int offset = -1;
	param_t * param = param_list_find_name(node, name);

	if (param == NULL) {
		printf("%s not found\n", name);
		return SLASH_EINVAL;
	}

	/* Check if Value is present */
	if (++argi >= slash->argc) {
		printf("missing parameter value\n");
		return SLASH_EINVAL;
	}
	
	char valuebuf[128] __attribute__((aligned(16))) = { };
	param_str_to_value(param->type, "10", valuebuf);

	/* Select destination, host overrides parameter node */
	int dest = node;

	if (param_push_single(param, offset, valuebuf, 1, dest, PIPELINE_CONFIG_TIMEOUT, 2) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	param_print(param, -1, NULL, 0, 2);

    return SLASH_SUCCESS;
}

slash_command_sub(pipeline, pipeline_conf, slash_csp_configure_pipeline, NULL, "Configure the pipeline stages");