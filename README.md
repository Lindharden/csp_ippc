# Cubesat Space Protocol - Image Processing Pipeline Configuration (csp_ippc)

Welcome to the csp_ippc repository! This repository contains CLI commands for CSH for configuring and managing the DISCO-2 image processing pipeline.

## Installation

csp_ippc is to be used as a submodule to CSH. After adding the submodule, register it as a dependency in meson.build with the following:

```
'csp_ippc:slash=true', ### add to default options ###
csp_ippc_dep = dependency('csp_ippc', fallback: ['csp_ippc', 'csp_ippc_dep'], required: true).as_link_whole() ### add this to csh dependencies ###
```

## Usage

### Command 1: `ippc pipeline`

This command updates what modules are to be active in the pipeline.
The default configuration file can be overridden using the `-f` flag.
Node should be given by \<env\> using the node command.

Usage:

```
ippc pipeline [options]
```

Options:

- `-n, --node [NUM]`: node (default = \<env\>).
- `-t, --timeout [NUM]`: timeout (default = \<env\>).
- `-f, --file [STRING]`: file (default = pipeline_config.yaml).
- `-v, --paramver`: parameter system version (default = 2).
- `-a, --no_ack_push`: Disable ack with param push queue (default = true).

Example:
The below example updates the pipeline configuration on node 162 using the specified yaml file.

```
ippc pipeline -n 162 -f pipeline_config.yaml
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
