#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param.h>
#include <param/param_list.h>
#include <yaml.h>

#include "include/csp_pipeline_config/pipeline_slash.h"
#include "include/csp_pipeline_config/pipeline_config.pb-c.h"
#include "include/csp_pipeline_config/module_config.pb-c.h"

void parse_pipeline_yaml_file(const char *filename, PipelineDefinition *pipeline)
{
	FILE *fh = fopen(filename, "r");
	yaml_parser_t parser;
	yaml_event_t event;

	/* Module definitions */
	ModuleDefinition *modules = NULL;
	int module_idx = -1;  // current module index
	int module_count = 0; // find total amount of modules

	/* Initialize parser */
	if (!yaml_parser_initialize(&parser))
		fputs("Failed to initialize parser!\n", stderr);
	if (fh == NULL)
		fputs("Failed to open file!\n", stderr);

	/* Set input file */
	yaml_parser_set_input_file(&parser, fh);

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
					fprintf(stderr, "Unexpected YAML scalar value format: %c.\n", event.data.scalar.value);
					break;
				}
			default:
				break;
		}
		if (event.type == YAML_SEQUENCE_END_EVENT)
			break;

		yaml_event_delete(&event);
	}

	// Insert ModuleDefinitions into PipelineDefinition
	pipeline->n_modules = module_count;
	pipeline->modules = malloc(sizeof(ModuleDefinition *) * module_count);
	for (size_t i = 0; i < module_count; i++)
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
	char *config_filename = "../yaml/ipp/pipeline_config.yaml";
	int node = PIPELINE_CSP_NODE_ID;
	optparse_t *parser = optparse_new("pconf", "");
	optparse_add_help(parser);
	optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = PIPELINE_CSP_NODE_ID)");
	optparse_add_string(parser, 'f', "file", "STRING", &config_filename, "file (default = pipeline_config.yaml)");

	if (strlen(config_filename) == 0)
	{
		printf("Config file path cannot be empty\n");
		return SLASH_EINVAL;
	}

	printf("Client: Configuring pipeline using %s\n", config_filename);

	// Define PipelineDefinition and parse yaml file
	PipelineDefinition pipeline = PIPELINE_DEFINITION__INIT;
	parse_pipeline_yaml_file(config_filename, &pipeline);

	// Pack PipelineDefinition
	size_t len_pipeline = pipeline_definition__get_packed_size(&pipeline);
    uint8_t packed_buf[len_pipeline + 1];
	pipeline_definition__pack(&pipeline, packed_buf);
	packed_buf[len_pipeline] = '\0'; // indicate end of data

	char *name = "pipeline_config";
	int offset = -1;
	param_t *param = param_list_find_name(node, name);

	if (param == NULL)
	{
		printf("%s not found\n", name);
		return SLASH_EINVAL;
	}

	// Insert packed pipeline definition into parameter
	if (param_push_single(param, offset, packed_buf, 1, node, PIPELINE_CONFIG_TIMEOUT, 2, true) < 0)
	{
		printf("No response\n");
		return SLASH_EIO;
	}

	param_print(param, -1, NULL, 0, 2, 0);

	return SLASH_SUCCESS;
}

slash_command_sub(ipp, pconf, slash_csp_configure_pipeline, NULL, "Configure the pipeline stages");

void parse_module_yaml_file(const char *filename, ModuleConfig *module_config)
{
	FILE *fh = fopen(filename, "r");
	yaml_parser_t parser;
	yaml_event_t event;

	/* Module definitions */
	ConfigParameter *params = NULL;
	int param_idx = -1;  // current module index
	int param_count = 0; // find total amount of modules

	/* Initialize parser */
	if (!yaml_parser_initialize(&parser))
		fputs("Failed to initialize parser!\n", stderr);
	if (fh == NULL)
		fputs("Failed to open file!\n", stderr);

	/* Set input file */
	yaml_parser_set_input_file(&parser, fh);

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
			case YAML_MAPPING_START_EVENT:
				// New Dash
				param_idx++;
				ConfigParameter *temp = realloc(params, (param_count + 1) * sizeof(ConfigParameter));
				if (!temp)
				{
					fprintf(stderr, "Failed to allocate memory for ConfigParameter during parsing\n");
					exit(EXIT_FAILURE);
				}
				params = temp; // Update the array pointer

				// Fill in the new struct
				ConfigParameter param = CONFIG_PARAMETER__INIT;
				params[param_idx] = param;

				param_count++; // Increment the number of elements
				break;
			case YAML_SCALAR_EVENT:
				// New set of ModuleDefinition encountered
				if (strcmp((char *)event.data.scalar.value, "key") == 0)
				{
					// Expect the next event to be the value of order
					if (!yaml_parser_parse(&parser, &event))
						break;	
					params[param_idx].key = strdup((char *)event.data.scalar.value);
				}
				else if (strcmp((char *)event.data.scalar.value, "type") == 0)
				{
					// Expect the next event to be the value of name
					if (!yaml_parser_parse(&parser, &event))
						break;
					params[param_idx].value_case = atoi((char *)event.data.scalar.value);
				}
				else if (strcmp((char *)event.data.scalar.value, "value") == 0)
				{
					// Expect the next event to be the value of param_id
					if (!yaml_parser_parse(&parser, &event))
						break;
					switch (params[param_idx].value_case)
					{
					case CONFIG_PARAMETER__VALUE_BOOL_VALUE:
						params[param_idx].bool_value = atoi((char *)event.data.scalar.value);
						break;
					case CONFIG_PARAMETER__VALUE_INT_VALUE:
						params[param_idx].int_value = atoi((char *)event.data.scalar.value);
						break;
					case CONFIG_PARAMETER__VALUE_FLOAT_VALUE:
						params[param_idx].float_value = atof((char *)event.data.scalar.value);
						break;
					case CONFIG_PARAMETER__VALUE_STRING_VALUE:
						params[param_idx].string_value = strdup((char *)event.data.scalar.value);
						break;
					default:
						// TODO: FAIL, unknown type!!!
						break;
					}
				}
				else
				{
					// Unexpected event type
					fprintf(stderr, "Unexpected YAML scalar value format: %c.\n", event.data.scalar.value);
					break;
				}
			default:
				break;
		}
		if (event.type == YAML_SEQUENCE_END_EVENT)
			break;

		yaml_event_delete(&event);
	}

	// Insert ConfigParameter into ModuleConfig
	module_config->n_parameters = param_count;
	module_config->parameters = malloc(sizeof(ConfigParameter *) * param_count);
	for (size_t i = 0; i < param_count; i++)
	{
		module_config->parameters[i] = &params[i];
	}

	/* Cleanup */
	yaml_event_delete(&event);
	yaml_parser_delete(&parser);
	fclose(fh);
}

static int slash_csp_configure_module(struct slash *slash)
{
	char *config_filename;
	int node = PIPELINE_CSP_NODE_ID;
	optparse_t *parser = optparse_new("mconf", "<module-idx>");
	optparse_add_help(parser);
	optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = PIPELINE_CSP_NODE_ID)");
	optparse_add_string(parser, 'f', "file", "STRING", &config_filename, "file (default = pipeline_module<module-idx>_config.yaml)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if module id is present */
	if (++argi >= slash->argc) {
		printf("missing module id\n");
		return SLASH_EINVAL;
	}

	/* Parse module id */
	int module_id = atoi(slash->argv[argi]);
	if (module_id < 1 && module_id > PIPELINE_MAX_MODULES)
	{
		printf("Module index is invalid. Range is 1-%d\n", PIPELINE_MAX_MODULES);
		return SLASH_EINVAL;
	}

	/* Set correct filepath according to module id */
	sprintf(config_filename, "../yaml/ipp/pipeline_module%d_config.yaml", module_id);

	printf("Client: Configuring pipeline module %d, using %s\n", module_id, config_filename);

	// Define PipelineDefinition and parse yaml file
	ModuleConfig module_config = MODULE_CONFIG__INIT;
	parse_module_yaml_file(config_filename, &module_config);

	// Pack PipelineDefinition
	size_t len_pipeline = module_config__get_packed_size(&module_config);
	uint8_t packed_buf[len_pipeline];
	pipeline_definition__pack(&module_config, packed_buf);

	char *name;
	sprintf(name, "module_param_%d", module_id);
	int offset = -1;
	param_t *param = param_list_find_name(node, name);

	if (param == NULL)
	{
		printf("%s not found\n", name);
		return SLASH_EINVAL;
	}

	char valuebuf[128] __attribute__((aligned(16))) = { };
	param_str_to_value(param->type, packed_buf, valuebuf);

	// Insert packed pipeline definition into parameter
	if (param_push_single(param, offset, valuebuf, 1, node, PIPELINE_CONFIG_TIMEOUT, 2) < 0)
	{
		printf("No response\n");
		return SLASH_EIO;
	}

	param_print(param, -1, NULL, 0, 2, 0);

	return SLASH_SUCCESS;
}

slash_command_sub(ipp, mconf, slash_csp_configure_module, NULL, "Configure a specific pipeline module");
