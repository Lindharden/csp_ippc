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

#include <csp_pipeline_config/pipeline_server.h>

static int slash_csp_configure_pipeline(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int timeout = PIPELINE_SERVER_TIMEOUT;

    optparse_t * parser = optparse_new("pipeline_conf", "<config-file>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timout", "NUM", 0, &timeout, "timout for connection (default = PIPELINE_SERVER_TIMEOUT)");

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

    return SLASH_SUCCESS;
}

slash_command_sub(pipeline, pipeline_conf, slash_csp_configure_pipeline, NULL, "Configure the pipeline stages");