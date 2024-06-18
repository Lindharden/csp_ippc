#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param.h>
#include <param/param_client.h>
#include <vmem/vmem_client.h>
#include <vmem/vmem_server.h>
#include <yaml.h>
#include <errno.h>
#include <math.h>
#include <jxl/decode.h>
#include <brotli/encode.h>

#include "pipeline_config.pb-c.h"
#include "module_config.pb-c.h"
#include "metadata.pb-c.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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
    optparse_add_unsigned(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
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
	size_t packed_size = pipeline_definition__get_packed_size(&pipeline);
	if (packed_size + 1 >= DATA_PARAM_SIZE) {
		printf("Packed configuration too large");
		return SLASH_EINVAL;
	}
	uint8_t packed_buf[packed_size];
	pipeline_definition__pack(&pipeline, packed_buf);

	size_t encoded_buffer_size = 1000;
    uint8_t encoded_buffer[encoded_buffer_size];
	if (BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, packed_size, packed_buf, &encoded_buffer_size, encoded_buffer) == BROTLI_FALSE)
    {
        fprintf(stderr, "Brotli compression failed\n");
		return SLASH_EINVAL;
    }
	if (encoded_buffer_size + 1 > DATA_PARAM_SIZE)
	{
		printf("Packed and encoded configuration is size %ld bytes which exceeds max size of %d\n", encoded_buffer_size + 1, DATA_PARAM_SIZE);
		return SLASH_EINVAL;
	}
	uint8_t buffer[encoded_buffer_size + 1];
	memcpy(buffer + 1, encoded_buffer, encoded_buffer_size);
	buffer[0] = encoded_buffer_size;

	// Mirror pipeline_config parameter
	int param_id = pipeline_id + PIPELINE_PARAMID_OFFSET - 1; // find correct pipeline id (Offset by +10 on pipeline server)
	PARAM_DEFINE_REMOTE_DYNAMIC(param_id, pipeline_config, node, PARAM_TYPE_DATA, DATA_PARAM_SIZE, 1, PM_CONF, &buffer, NULL);

	// The parameter name must have the id
	char name[20];
	sprintf(name, "pipeline_config_%d", pipeline_id);
	pipeline_config.name = name;

	// Insert packed pipeline definition into parameter
	if (param_push_single(&pipeline_config, -1, buffer, 0, node, timeout, paramver, ack_with_pull) < 0)
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
					if (safe_atoi((char *)event.data.scalar.value, (int *)&params[param_idx].value_case) < 0)
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
    optparse_add_unsigned(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
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
	size_t packed_size = module_config__get_packed_size(&module_config);
	if (packed_size + 1 >= DATA_PARAM_SIZE) {
		printf("Packed configuration too large");
		return SLASH_EINVAL;
	}
	uint8_t packed_buf[packed_size];
	module_config__pack(&module_config, packed_buf); // Insert after first index
	
	size_t encoded_buffer_size = 1000;
    uint8_t encoded_buffer[encoded_buffer_size];
	if (BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, packed_size, packed_buf, &encoded_buffer_size, encoded_buffer) == BROTLI_FALSE)
    {
        fprintf(stderr, "Brotli compression failed\n");
		return SLASH_EINVAL;
    }
	if (encoded_buffer_size + 1 > DATA_PARAM_SIZE)
	{
		printf("Packed and encoded configuration is size %ld bytes which exceeds max size of %d\n", encoded_buffer_size + 1, DATA_PARAM_SIZE);
		return SLASH_EINVAL;
	}
	uint8_t buffer[encoded_buffer_size + 1];
	memcpy(buffer + 1, encoded_buffer, encoded_buffer_size);
	buffer[0] = encoded_buffer_size;

	// Mirror module_param parameter
	int param_id = module_id + MODULE_PARAMID_OFFSET - 1; // find correct parameter id (Offset by +30 on pipeline server)
	PARAM_DEFINE_REMOTE_DYNAMIC(param_id, module_param, node, PARAM_TYPE_DATA, DATA_PARAM_SIZE, 1, PM_CONF, &buffer, NULL);

	// The parameter name must have the id
	char name[20];
	sprintf(name, "module_param_%d", module_id);
	module_param.name = name;

	// Insert packed pipeline definition into parameter
	if (param_push_single(&module_param, -1, buffer, 0, node, timeout, paramver, ack_with_pull) < 0)
	{
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}

slash_command_sub(ippc, module, slash_csp_configure_module, "[OPTIONS...] <module-idx> <config-file>", "Configure a specific module");

static MetadataItem *get_item(Metadata *data, const char *key) {
    MetadataItem *found_item = NULL;
    for (size_t i = 0; i < data->n_items; i++)
    {
        if (strcmp(data->items[i]->key, key) == 0)
        {
            found_item = data->items[i];
            break;
        }
    }
    return found_item;
}

char *get_custom_metadata_string(Metadata *data, char *key)
{
    MetadataItem *found_item = get_item(data, key);

    if (found_item == NULL)
    {
        return NULL;
    }
    
    return found_item->string_value;
}

static int slash_csp_buffer_get(struct slash *slash)
{
	unsigned int node = slash_dfl_node; // fetch current node id
    unsigned int timeout = slash_dfl_timeout;
	unsigned int paramver = 2;
	int ack_with_pull = true;
	int save_png = false;
	int front = false;
	optparse_t *parser = optparse_new("get", "<offset>");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");
	optparse_add_set(parser, 's', "save_png", 1, &save_png, "Save downloaded data as png (default = false)");
	optparse_add_set(parser, 'f', "front", 1, &front, "Index from front/newest image (default = false)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0)
	{
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if tail offset is present */
	if (++argi >= slash->argc)
	{
		printf("Missing tail offset\n");
		return SLASH_EINVAL;
	}

	/* Fetch tail offset parameter */
	int input_offset = atoi(slash->argv[argi]);
	if (front) input_offset *= -1;

	/* Download image file */
	unsigned char *image_data = (unsigned char *)malloc(10000000); // image buffer
	int size_down = vmem_ring_download(node, timeout, "images", input_offset, (char *)image_data, 2, 1);
	if (size_down == -1) {
		printf("Download failed\n");
		return SLASH_EINVAL;
	}
	printf("Downloaded %d bytes from node %d in ring buffer '%s' at offset %d\n", size_down, node, "images", input_offset);
	
	/* Extract image metadata */
	size_t offset = 0;
	uint32_t metadata_size = *((uint32_t *)(image_data)); 
	offset += sizeof(uint32_t);
	Metadata *meta = metadata__unpack(NULL, metadata_size, (uint8_t *) image_data + offset); 
	offset += metadata_size;
	uint32_t image_data_size = meta->size;

	char *enc = get_custom_metadata_string(meta, "enc");
	int is_encoded = enc != NULL && !strcmp(enc, "jxl");
	printf("Encoded: %d\n", is_encoded);
	int width = meta->width;
	int height = meta->height;
	int channels = meta->channels;
	int stride = width * channels;
	uint8_t *data = image_data + offset;

	if (is_encoded)
	{
		/* Decode image data using JXL */
		JxlDecoder* decoder = JxlDecoderCreate(NULL);
		if (JxlDecoderSetInput(decoder, image_data + offset, image_data_size) == JXL_DEC_ERROR)
		{
			printf("Error: Could not decode image\n");
			return SLASH_EINVAL;
		}

		JxlBasicInfo basic_info;
		size_t buffer_size;
		JxlPixelFormat format;
		uint8_t combined_channels;
		JxlDecoderSubscribeEvents(decoder, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE | JXL_DEC_BASIC_INFO);
		
		while (1)
		{
			JxlDecoderStatus status = JxlDecoderProcessInput(decoder);

			if (status == JXL_DEC_ERROR)
			{
				printf("Error: Jxl decoder error\n");
				return SLASH_EINVAL;
			}

			if (status == JXL_DEC_SUCCESS) 
			{
				break;
			}
			
			if (status == JXL_DEC_FULL_IMAGE) 
			{
				break;
			}

			if (status == JXL_DEC_BASIC_INFO) 
			{
				JxlDecoderGetBasicInfo(decoder, &basic_info);
				combined_channels = basic_info.num_color_channels + basic_info.num_extra_channels;
				format.num_channels = combined_channels; format.data_type = JXL_TYPE_UINT8; 
				format.endianness = JXL_NATIVE_ENDIAN; format.align = 0;
			}
			
			if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) 
			{
				JxlDecoderImageOutBufferSize(decoder, &format, &buffer_size);
				data = (uint8_t *)malloc(buffer_size);
				JxlDecoderSetImageOutBuffer(decoder, &format, data, buffer_size);
			}
		}

		if (basic_info.xsize != width || basic_info.ysize != height || combined_channels != channels)
		{
			printf("Info: Dimensions given by metadata do not match decoded dimensions\n");
		}
	}

	if (save_png)
	{
		/* Save decoded image data */
		char filename[20];
		sprintf(filename, "image_%s.png", meta->camera);
		int write_success = stbi_write_png(filename, width, height, channels, data, stride);
		if (!write_success)
		{
			fprintf(stderr, "Error writing image to %s\n", filename);
			return SLASH_EINVAL;
		}
		else
		{
			printf("Image saved as %s\n", filename);
		}
	}
	return SLASH_SUCCESS;
}

slash_command_sub(ippb, get, slash_csp_buffer_get, "[OPTIONS...] <offset>", "Fetch image at <offset> from the DISCO-2 ring-buffer (0 = oldest, -1 = newest)");


