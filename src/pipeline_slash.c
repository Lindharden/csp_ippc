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
#include "include/csp_pipeline_config/yaml.h"
#include "include/csp_pipeline_config/pipeline_config.pb-c.h"

void parse_yaml_file(const char *filename, PipelineDefinition *pipeline)
{
	FILE *fh = fopen(filename, "r");
	yaml_parser_t parser;
	yaml_event_t event;

	/* Initialize parser */
	if (!yaml_parser_initialize(&parser))
		fputs("Failed to initialize parser!\n", stderr);
	if (fh == NULL)
		fputs("Failed to open file!\n", stderr);

	/* Set input file */
	yaml_parser_set_input_file(&parser, fh);

	/* Module definitions */
	ModuleDefinition *modules = NULL;
	int module_idx = -1;  // current module index
	int module_count = 0; // find total amount of modules

	/* START parsing */
	while (1)
	{
		if (!yaml_parser_parse(&parser, &event))
		{
			printf("Parser error %d\n", parser.error);
			exit(EXIT_FAILURE);
		}

		switch (event.type)
		{
		case YAML_SEQUENCE_START_EVENT:
			break;
		case YAML_MAPPING_START_EVENT:
			// New Dash
			module_idx++;
			ModuleDefinition *temp = realloc(modules, (module_count + 1) * sizeof(ModuleDefinition));
			if (!temp)
			{
				fprintf(stderr, "Failed to allocate memory for ModuleDefinition during parsing\n");
				exit(EXIT_FAILURE);
			}
			modules = temp; // Update the array pointer

			// Fill in the new struct
			ModuleDefinition module = MODULE_DEFINITION__INIT;
			modules[module_idx] = module;

			module_count++; // Increment the number of elements
			break;
		case YAML_SCALAR_EVENT:
			// New set of ModuleDefinition encountered
			if (strcmp((char *)event.data.scalar.value, "order") == 0)
			{
				// Expect the next event to be the value of order
				if (!yaml_parser_parse(&parser, &event))
					break;
				modules[module_idx].order = atoi((char *)event.data.scalar.value);
			}
			else if (strcmp((char *)event.data.scalar.value, "name") == 0)
			{
				// Expect the next event to be the value of name
				if (!yaml_parser_parse(&parser, &event))
					break;
				modules[module_idx].name = strdup((char *)event.data.scalar.value);
			}
			else if (strcmp((char *)event.data.scalar.value, "param_id") == 0)
			{
				// Expect the next event to be the value of param_id
				if (!yaml_parser_parse(&parser, &event))
					break;
				modules[module_idx].param_id = atoi((char *)event.data.scalar.value);
			}
			else
			{
				// Unexpected event type
				fprintf(stderr, "Unexpected YAML scalar value format.\n");
				break;
			}
		default:
			break;
		}
		if (event.type == YAML_STREAM_END_EVENT)
			break;

		yaml_event_delete(&event);
	}

	// Insert ModuleDefinitions into PipelineDefinition
	pipeline->n_modules = module_idx;
	for (size_t i = 0; i < module_idx; i++)
	{
		pipeline->modules[i] = &modules[i];
	}

	/* Cleanup */
	yaml_event_delete(&event);
	yaml_parser_delete(&parser);
	fclose(fh);
}

static int slash_csp_configure_pipeline(struct slash *slash)
{
	char *config_filename = "../../yaml/pipeline_config.yaml";
	int node = PIPELINE_CSP_NODE_ID;
	optparse_t *parser = optparse_new("pconf", "");
	optparse_add_help(parser);
	optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = PIPELINE_CSP_NODE_ID)");
	optparse_add_string(parser, 'f', "file", "STRING", 0, &config_filename, "node (default = template.yaml)");

	if (strlen(config_filename) == 0)
	{
		printf("Config file path cannot be empty\n");
		return SLASH_EINVAL;
	}

	printf("Client: Configuring pipeline using %s\n", config_filename);

	// Define PipelineDefinition and parse yaml file
	PipelineDefinition pipeline = PIPELINE_DEFINITION__INIT;
	parse_yaml_file(config_filename, &pipeline);

	// Pack PipelineDefinition
	size_t len_pipeline = pipeline_definition__get_packed_size(&pipeline);
	uint8_t packed_buf[len_pipeline];
	pipeline_definition__pack(&pipeline, packed_buf);

	char *name = "pipeline_config";
	int offset = -1;
	param_t *param = param_list_find_name(node, name);

	if (param == NULL)
	{
		printf("%s not found\n", name);
		return SLASH_EINVAL;
	}

	// Insert packed pipeline definition into parameter
	if (param_push_single(param, offset, packed_buf, 1, node, PIPELINE_CONFIG_TIMEOUT, 2) < 0)
	{
		printf("No response\n");
		return SLASH_EIO;
	}

	param_print(param, -1, NULL, 0, 2);

	return SLASH_SUCCESS;
}

slash_command_sub(ipp, pconf, slash_csp_configure_pipeline, NULL, "Configure the pipeline stages");
