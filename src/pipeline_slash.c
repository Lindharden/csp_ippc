#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param.h>
#include <yaml.h>
#include <errno.h>
#include <math.h>

#include "include/csp_pipeline_config/pipeline_config.pb-c.h"
#include "include/csp_pipeline_config/module_config.pb-c.h"

#define MAX_MODULES 20
#define MAX_PIPELINES 6
#define PIPELINE_PARAMID_OFFSET 10
#define MODULE_PARAMID_OFFSET 30
#define DATA_PARAM_SIZE 188

int initialize_parser(const char *filename, yaml_parser_t *parser, FILE *fh)
{
	fh = fopen(filename, "r");

	/* Initialize parser */
	if (!yaml_parser_initialize(parser))
	{
		fprintf(stderr, "Error: Failed to initialize parser!\n");
		return -1;
	}
	if (fh == NULL)
	{
		fprintf(stderr, "Error: Failed to open file!\n");
		return -1;
	}

	/* Set input file */
	yaml_parser_set_input_file(parser, fh);

	return 0;
}

void cleanup_resources(yaml_parser_t *parser, yaml_event_t *event, FILE *fh)
{
	if (event != NULL)
	{
		yaml_event_delete(event);
	}
	yaml_parser_delete(parser);

	if (fh != NULL)
	{
		fclose(fh);
	}
}

int safe_atof(const char *in, float *out)
{
	errno = 0; // To detect overflow or underflow
	char *endptr;
	float val = strtof(in, &endptr);

	if (endptr == in)
	{
		fprintf(stderr, "Error: Value \"%s\" could not be parsed to a floating-point number.\n", in);
		return -1;
	}
	else if (*endptr != '\0')
	{
		fprintf(stderr, "Error: Extra characters after number: \"%s\"\n", endptr);
		return -1;
	}
	else if ((val == HUGE_VALF || val == -HUGE_VALF) && errno == ERANGE)
	{
		// Note: Checking against HUGE_VALF for float-specific overflow/underflow
		fprintf(stderr, "Error: Value \"%s\" is out of range.\n", in);
		return -1;
	}

	*out = val;
	return 0;
}

int safe_atoi(const char *in, int *out)
{
	errno = 0; // To detect overflow
	char *endptr;
	long val = strtol(in, &endptr, 10); // Base 10 for decimal conversion

	if (endptr == in)
	{
		fprintf(stderr, "Error: No digits were found in token %s\n", in);
		return -1;
	}
	if (*endptr != '\0')
	{
		fprintf(stderr, "Error: Extra characters after number: %s in token %s\n", endptr, in);
		return -1;
	}
	if ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)
	{
		fprintf(stderr, "Error: Value out of long int range for token %s\n", in);
		return -1;
	}
	if (val < INT_MIN || val > INT_MAX)
	{
		fprintf(stderr, "Error: Value out of int range for token %s\n", in);
		return -1;
	}

	*out = (int)val;
	return 0;
}

int parse_pipeline_yaml_file(const char *filename, PipelineDefinition *pipeline)
{
	yaml_parser_t parser;
	FILE *fh = NULL;
	if (initialize_parser(filename, &parser, fh) < 0)
		return -1;

	/* Module definitions */
	ModuleDefinition *modules = NULL;
	int module_idx = -1;  // current module index
	int module_count = 0; // find total amount of modules
    int parsing_modules = 0; // Flag to indicate if we are parsing modules

	/* START parsing */
	yaml_event_t event;
	while (1)
	{
		if (!yaml_parser_parse(&parser, &event))
		{
			printf("Error: Parser error %d\n", parser.error);
			return -1;
		}
		switch (event.type)
		{
			case YAML_MAPPING_START_EVENT:
				if (!parsing_modules) break;
				// New Dash
				module_idx++;
				ModuleDefinition *temp = realloc(modules, (module_count + 1) * sizeof(ModuleDefinition));
				if (!temp)
				{
					fprintf(stderr, "Error: Failed to allocate memory for ModuleDefinition during parsing\n");
					return -1;
				}

				modules = temp;

				// Fill in the new struct
				ModuleDefinition module = MODULE_DEFINITION__INIT;
				modules[module_idx] = module;

				module_count++;
				break;
			case YAML_SCALAR_EVENT:
				if (strcmp((char *)event.data.scalar.value, "pipeline_name") == 0)
				{
					// Expect the next event to be the value of name
                    if (!yaml_parser_parse(&parser, &event))
                        break;
                    pipeline->name = strdup((char *)event.data.scalar.value);
					break;
				}
				else if (strcmp((char *)event.data.scalar.value, "modules") == 0)
				{
					parsing_modules = 1;
					break;
				}
				// New set of ModuleDefinition encountered
				if (strcmp((char *)event.data.scalar.value, "order") == 0)
				{
					// Expect the next event to be the value of order
					if (!yaml_parser_parse(&parser, &event))
						break;
					if (safe_atoi((char *)event.data.scalar.value, &modules[module_idx].order) < 0)
					{
						return -1;
					}
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
					if (safe_atoi((char *)event.data.scalar.value, &modules[module_idx].param_id) < 0)
					{
						return -1;
					}
					if (modules[module_idx].param_id < 1 || modules[module_idx].param_id > MAX_MODULES) {
						fprintf(stderr, "Error: Param_id is invalid. Range is 1-%d\n", MAX_MODULES);
						return -1;
					}
				}
				else
				{
					// Unexpected event type
					fprintf(stderr, "Error: Unexpected YAML scalar value format: %s\nAllowed values are: order, name, param_id\n", (char *)event.data.scalar.value);
					return -1;
				}
			default:
				break;
		}
		if (event.type == YAML_SEQUENCE_END_EVENT || event.type == YAML_DOCUMENT_END_EVENT)
			break;

		yaml_event_delete(&event);
	}

	// Insert ModuleDefinitions into PipelineDefinition
	pipeline->n_modules = module_count;
	pipeline->modules = malloc(sizeof(ModuleDefinition *) * module_count);
	int orders[module_count];
	for (size_t i = 0; i < module_count; i++)
	{
		pipeline->modules[i] = &modules[i];

		// Ensure module orders are unique
		if (modules[i].order > module_count || modules[i].order < 1 || orders[modules[i].order - 1] == 1) {
			fprintf(stderr, "Error: Order values are out of bounds or not sequential, bad value: %d\n", modules[i].order);
			return -1;
		}
		orders[modules[i].order - 1] = 1;
	}

	cleanup_resources(&parser, &event, fh);

	return 0;
}

static int slash_csp_configure_pipeline(struct slash *slash)
{
	unsigned int node = slash_dfl_node; // fetch current node id
    unsigned int timeout = slash_dfl_timeout;
	unsigned int paramver = 2;
	int ack_with_pull = true;
	optparse_t *parser = optparse_new("pipeline", "<pipeline-idx> <config-file>");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0)
	{
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if pipeline id is present */
	if (++argi >= slash->argc)
	{
		printf("Missing pipeline id\n");
		return SLASH_EINVAL;
	}

	/* Parse pipeline id */
	int pipeline_id = atoi(slash->argv[argi]);
	if (pipeline_id < 1 || pipeline_id > MAX_PIPELINES)
	{
		printf("Pipeline index is invalid. Range is 1-%d\n", MAX_PIPELINES);
		return SLASH_EINVAL;
	}

	/* Check if config file is present */
	if (++argi >= slash->argc)
	{
		printf("Missing config-file path\n");
		return SLASH_EINVAL;
	}

	/* Parse config file */
	char *config_filename = strdup(slash->argv[argi]);

	printf("Client: Configuring pipeline %d, using %s\n", pipeline_id, config_filename);

	// Define PipelineDefinition and parse yaml file
	PipelineDefinition pipeline = PIPELINE_DEFINITION__INIT;
	if (parse_pipeline_yaml_file(config_filename, &pipeline) < 0)
	{
		return SLASH_EINVAL;
	}

	// Pack PipelineDefinition
	size_t len_pipeline = pipeline_definition__get_packed_size(&pipeline);
	if (len_pipeline + 1 >= DATA_PARAM_SIZE) {
		printf("Packed configuration too large");
		return SLASH_EINVAL;
	}
	uint8_t packed_buf[len_pipeline + 1];
	pipeline_definition__pack(&pipeline, &packed_buf[1]); // Insert after first index
	packed_buf[0] = len_pipeline; // Insert data length in first index

	// Mirror pipeline_config parameter
	int param_id = pipeline_id + PIPELINE_PARAMID_OFFSET - 1; // find correct pipeline id (Offset by +10 on pipeline server)
	PARAM_DEFINE_REMOTE_DYNAMIC(param_id, pipeline_config, node, PARAM_TYPE_DATA, DATA_PARAM_SIZE, 1, PM_CONF, &packed_buf, NULL);

	// The parameter name must have the id
	char name[20];
	sprintf(name, "pipeline_config_%d", pipeline_id);
	pipeline_config.name = name;

	// Insert packed pipeline definition into parameter
	if (param_push_single(&pipeline_config, -1, packed_buf, 0, node, timeout, paramver, ack_with_pull) < 0)
	{
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}

slash_command_sub(ippc, pipeline, slash_csp_configure_pipeline, "[OPTIONS...] <pipeline-idx> <config-file>", "Configure a specific pipeline");

int parse_module_yaml_file(const char *filename, ModuleConfig *module_config)
{
	yaml_parser_t parser;
	FILE *fh = NULL;
	if (initialize_parser(filename, &parser, fh) < 0)
		return -1;

	/* Module definitions */
	ConfigParameter *params = NULL;
	int param_idx = -1;	 // current module index
	int param_count = 0; // find total amount of modules

	/* START parsing */
	yaml_event_t event;
	while (1)
	{
		if (!yaml_parser_parse(&parser, &event))
		{
			printf("Error: Parser error %d\n", parser.error);
			return -1;
		}
		switch (event.type)
		{
			case YAML_MAPPING_START_EVENT:
				// New Dash
				param_idx++;
				ConfigParameter *temp = realloc(params, (param_count + 1) * sizeof(ConfigParameter));
				if (!temp)
				{
					fprintf(stderr, "Error: Failed to allocate memory for ConfigParameter during parsing\n");
					return -1;
				}
				params = temp;

				// Fill in the new struct
				ConfigParameter param = CONFIG_PARAMETER__INIT;
				params[param_idx] = param;

				param_count++;
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
					if (safe_atoi((char *)event.data.scalar.value, &params[param_idx].value_case) < 0)
					{
						return -1;
					}
				}
				else if (strcmp((char *)event.data.scalar.value, "value") == 0)
				{
					// Expect the next event to be the value of param_id
					if (!yaml_parser_parse(&parser, &event))
						break;
					switch (params[param_idx].value_case)
					{
						case CONFIG_PARAMETER__VALUE_BOOL_VALUE:
							if (strcmp((char *)event.data.scalar.value, "true") == 0)
							{
								params[param_idx].bool_value = 1;
							}
							else if (strcmp((char *)event.data.scalar.value, "false") == 0)
							{
								params[param_idx].bool_value = 0;
							}
							else
							{
								fprintf(stderr, "Error: Could not parse %s to boolean value\nAllowed values are: true, false\n", (char *)event.data.scalar.value);
								return -1;
							}
							break;
						case CONFIG_PARAMETER__VALUE_INT_VALUE:
							if (safe_atoi((char *)event.data.scalar.value, &params[param_idx].int_value) < 0)
							{
								return -1;
							}
							break;
						case CONFIG_PARAMETER__VALUE_FLOAT_VALUE:
							if (safe_atof((char *)event.data.scalar.value, &params[param_idx].float_value) < 0)
							{
								return -1;
							}
							break;
						case CONFIG_PARAMETER__VALUE_STRING_VALUE:
							params[param_idx].string_value = strdup((char *)event.data.scalar.value);
							break;
						default:
							fprintf(stderr, "Error: Value case %d unknown.\n", params[param_idx].value_case);
							return -1;
					}
				}
				else
				{
					// Unexpected event type
					printf("Error: Unexpected YAML scalar value format: %s\nAllowed values are: key, type, value\n", (char *)event.data.scalar.value);
					return -1;
				}
			default:
				break;
		}
		if (event.type == YAML_SEQUENCE_END_EVENT || event.type == YAML_DOCUMENT_END_EVENT)
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

	cleanup_resources(&parser, &event, fh);

	return 0;
}

static int slash_csp_configure_module(struct slash *slash)
{
	unsigned int node = slash_dfl_node; // fetch current node id
    unsigned int timeout = slash_dfl_timeout;
	unsigned int paramver = 2;
	int ack_with_pull = true;
	optparse_t *parser = optparse_new("module", "<module-idx> <config-file>");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0)
	{
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if module id is present */
	if (++argi >= slash->argc)
	{
		printf("Missing module id\n");
		return SLASH_EINVAL;
	}

	/* Parse module id */
	int module_id = atoi(slash->argv[argi]);
	if (module_id < 1 || module_id > MAX_MODULES)
	{
		printf("Module index is invalid. Range is 1-%d\n", MAX_MODULES);
		return SLASH_EINVAL;
	}

	/* Check if config file is present */
	if (++argi >= slash->argc)
	{
		printf("Missing config-file path\n");
		return SLASH_EINVAL;
	}

	/* Parse config file */
	char *config_filename = strdup(slash->argv[argi]);

	printf("Client: Configuring module %d, using %s\n", module_id, config_filename);

	// Define PipelineDefinition and parse yaml file
	ModuleConfig module_config = MODULE_CONFIG__INIT;
	if (parse_module_yaml_file(config_filename, &module_config) < 0)
		return SLASH_EINVAL;

	// Pack PipelineDefinition
	size_t len_module = module_config__get_packed_size(&module_config);
	if (len_module + 1 >= DATA_PARAM_SIZE) {
		printf("Packed configuration too large");
		return SLASH_EINVAL;
	}
	uint8_t packed_buf[len_module + 1];
	module_config__pack(&module_config, &packed_buf[1]); // Insert after first index
	packed_buf[0] = len_module; // Insert data length in first index

	// Mirror module_param parameter
	int param_id = module_id + MODULE_PARAMID_OFFSET - 1; // find correct parameter id (Offset by +30 on pipeline server)
	PARAM_DEFINE_REMOTE_DYNAMIC(param_id, module_param, node, PARAM_TYPE_DATA, DATA_PARAM_SIZE, 1, PM_CONF, &packed_buf, NULL);

	// The parameter name must have the id
	char name[20];
	sprintf(name, "module_param_%d", module_id);
	module_param.name = name;

	// Insert packed pipeline definition into parameter
	if (param_push_single(&module_param, -1, packed_buf, 0, node, timeout, paramver, ack_with_pull) < 0)
	{
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}

slash_command_sub(ippc, module, slash_csp_configure_module, "[OPTIONS...] <module-idx> <config-file>", "Configure a specific module");
