# Cubesat Space Protocol - Image Processing Pipeline Configuration (csp_ippc)

Welcome to the csp_ippc repository! This repository contains CLI commands for CSH for configuring and managing the DISCO-2 image processing pipeline.

## Installation

csp_ippc is to be used as a submodule to CSH. After adding the submodule, register it as a dependency in meson.build with the following:

```
'csp_ippc:slash=true', ### add to default options ###
csp_ippc_dep = dependency('csp_ippc', fallback: ['csp_ippc', 'csp_ippc_dep'], required: true).as_link_whole() ### add this to csh dependencies ###
```

## Dependencies

csp_ippc has a few dependencies namely: libprotobuf-c libjxl libbrotlienc

## Usage

### Command 1: `ippc pipeline`

This command updates what modules are to be active in the specified pipeline.
Specify id of pipeline and yaml config file (relative path).
Node should be given by \<env\> using the node command.

Usage:

```
ippc pipeline [options] <pipeline-idx> <config-file>
```

Options:

- `-n, --node [NUM]`: node (default = \<env\>).
- `-t, --timeout [NUM]`: timeout (default = \<env\>).
- `-v, --paramver`: parameter system version (default = 2).
- `-a, --no_ack_push`: Disable ack with param push queue (default = true).

Example:
The below example updates the pipeline configuration for pipeline 1 on node 162 using the specified yaml file.

```
ippc pipeline -n 162 1 "pipeline_config.yaml"
```

Example of a valid pipeline configuration file:

```yaml
- order: 1
  param_id: 1
  name: flip_horizontally
- order: 2
  param_id: 2
  name: bw_convert
```

### Command 2: `ippc module`

This command updates the parameters for a specific pipeline module.
Specify id of module and yaml config file (relative path).
Node should be given by \<env\> using the node command.

Usage:

```
ippc module [options] <module-idx> <config-file>
```

Options:

- `-n, --node [NUM]`: node (default = \<env\>).
- `-t, --timeout [NUM]`: timeout (default = \<env\>).
- `-v, --paramver`: parameter system version (default = 2).
- `-a, --no_ack_push`: Disable ack with param push queue (default = true).

Example:
The below example update the parameters for module 2 on node 162 using the specified yaml file.

```
ippc module -n 162 2 "module_config.yaml"
```

Example of a valid module configuration file:

```yaml
- key: "flip_percent"
  type: 4
  value: 0.50

- key: "flip_amount"
  type: 3
  value: 1
```

### Command 3: `ippb get`

This command downloads an observation from the ring buffer.
Specify index of the desired observation (0 = oldest).
Node should be given by \<env\> using the node command.

Usage:

```
ippb get [options] <index>
```

Options:

- `-n, --node [NUM]`: node (default = \<env\>).
- `-t, --timeout [NUM]`: timeout (default = \<env\>).
- `-v, --paramver`: parameter system version (default = 2).
- `-a, --no_ack_push`: Disable ack with param push queue (default = true).

Example:
The below example downloads the second oldest observation stored in the ring buffer on node 150.

```
ippb get -n 150 1
```

If the observation metadata specifies jxl encoding, the data will be decoded.