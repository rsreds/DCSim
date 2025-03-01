# DistCacheSim

Simulator for the simulation of high energy physics workloads on distributed computing systems with caching.


## Install instructions

### Option 1
To get a fresh installation on your own system, either `git clone` this repository and execute the checkout script inside:
```bash
source checkout_scripts/checkout_with_sudo.sh
```
Mind that you will need super-user rights to do so, as well as `cmake`, `git`, `clang` and `boost` installed on your system.
This will install the executable `dc-sim` for this simulator and all its software dependencies.

### Option 2
Create a `conda` environment using the provided script
```bash
checkout_scripts/install_conda_environment.sh
```
This will automatically take care of all the dependencies needed and include them into the environment.

To work with this environment interactively, you first have to initialize conda on your system. This can be done via:

```bash
</path/to/your/conda/installation>/bin/conda init
```

This would adapt your `~/.bashrc` to be able to call `conda` directly. So please re-open your shell or `source ~/.bashrc`.

To activate the environment, execute
```bash
conda activate dcsim-env
```
and deactivate it accordingly with 
```bash
conda deactivate
```

### Tips for Conda

With a `conda` environment, you would be able to install the full software setup without super-user rights.
More information on how to work with and develop in a `conda` environment can be found in the [Conda Documentation](https://docs.anaconda.com/)

Furthermore, it is possible to put a complete conda environment into a tarball to be able to export it to a different machine, e.g. a batch system node. To do that execute:

```bash
conda activate dcsim-env # in case you don't have it activated yet
conda-pack
```

The created tarball `dcsim-env.tar.gz` can then be uploaded to a storage element and copied from there to a different machine.


## Usage
When you have successfully installed the simulator or activated the conda environment you can run
```bash
dc-sim --help
```
to see all possible execution options. 

Mandatory parameters are a platform file and a path and name for the resulting simulation output CSV-file:
```bash
dc-sim -p <platform-file> -o <output-path>
```
The platform file has to follow the [SimGrid-defined DTD](https://simgrid.org/doc/latest/Platform.html).
Example files can be found in `data/platform-files`.
The output-path can be any relative or absolute path of your file-system where you are allowed to write to.
Instead of manually setting up all workload parameters via command line options, 
there is also the option to provide a JSON file, which contains all necessary information about one or multiple workloads by adding the option:
```bash
--workload-configurations <path_to_workload_json>
```
The workloads have to contain the full information. 
Opposed to as it would be set via the command line, where only gaussian distributed job characteristics are supported, in the workload file also distributions according to a histogram can be used.
An example for a workload mixing both gaussian and histogram distributions of its job characteristics would be, e.g.:
```json
{
    "calc_workload": {
        "num_jobs": 60,
        "infiles_per_job": 0,
        "flops": {
            "type": "histogram",
            "bins": [1164428000000,2164428000000,3164428000000],
            "counts": [50,50]
        },
        "memory": {
            "type": "gaussian",
            "average": 2000000000,
            "sigma": 200000000
        },
        "infilesize": {
            "type": "gaussian",
            "average": 0,
            "sigma": 0
        },
        "outfilesize": {
            "type": "gaussian",
            "average": 18000000,
            "sigma": 1800000
        },
        "workload_type": "calculation",
        "submission_time": 0
    }
}
```
It is also possible to give a list of workload configuration files and configure more than one workload per file, which enables to simulate the execution of multiple sets of workloads in the same simulation run.
Example configurations covering different workload-types is given in `data/workload-configs/workload_testsuite.json`.
