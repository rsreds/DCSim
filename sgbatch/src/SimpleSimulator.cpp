/**
 * Copyright (c) 2020. <ADD YOUR HEADER INFORMATION>.
 * Generated with the wrench-init.in tool.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include <wrench.h>
#include "SimpleWMS.h"


std::mt19937 gen(42);

/**
 * @brief helper function wich checks if a string ends with a desired suffix
 * 
 * @param str: string to check
 * @param suffix: suffix to match to
 */
static bool ends_with(const std::string& str, const std::string& suffix) {
  return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

/**
 * @brief helper function for converting a CLI argument string to double
 * 
 * @param arg: a CLI argument string
 * 
 * @return the converted argument
 */
double arg_to_double (const std::string& arg) {
  try {
    std::size_t pos;
    double value = std::stod(arg, &pos);
    if (pos < arg.size()) {
      std::cerr << "Trailing characters after number: " << arg << std::endl;
    }
    return value;
  } catch (const std::invalid_argument& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "Invalid number: " << arg << std::endl;
    exit (EXIT_FAILURE);
  } catch (const std::out_of_range& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "Number out of range: " << arg << std::endl;
    exit (EXIT_FAILURE);
  }
}

/**
 * @brief helper function for converting CLI argument to size_t
 *
 * @param arg: a CLI argument string
 * 
 * @return the converted argument
 */
size_t arg_to_sizet (const std::string& arg) {
  try {
    std::size_t pos;
    size_t value = std::stoi(arg, &pos);
    if (pos < arg.size()) {
      std::cerr << "Trailing characters after number: " << arg << std::endl;
    }
    return value;
  } catch (const std::invalid_argument& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "Invalid number: " << arg << std::endl;
    exit (EXIT_FAILURE);
  } catch (const std::out_of_range& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "Number out of range: " << arg << std::endl;
    exit (EXIT_FAILURE);
  }
}


/**
 * @brief fill a Workflow with tasks, which stream input data and perform computations in blocks
 * 
 * 
 * @param workflow: Workflow to fill with tasks
 * @param blockstreaming: switch to turn on blockwise streaming of input data
 * @param xrd_block_size: maximum size of the streamed file blocks in bytes for the XRootD-ish streaming
 * @param dummy_flops: numer of lops each dummy task is executing
 * @param num_jobs: number of tasks
 * @param infiles_per_task: number of input-files each job processes
 * @param average_flops: expectation value of the flops distribution
 * @param sigma_flops: std. deviation of the flops distribution
 * @param average_memory: expectation value of the memory distribution
 * @param sigma_memory: std. deviation of the memory distribution
 * @param average_infile_size: expectation value of the input-file size distribution
 * @param sigma_infile_size: std. deviation of the input-file size distribution
 */
void fill_streaming_workflow (
  wrench::Workflow* workflow,
  size_t num_jobs,
  size_t infiles_per_task,
  double average_flops, double sigma_flops,
  double average_memory, double sigma_memory,
  double average_infile_size, double sigma_infile_size,
  const bool use_blockstreaming = true,
  double xrd_block_size = 1*1000*1000,
  const double dummy_flops = std::numeric_limits<double>::min()
) {
  // Initialize random number generators
  std::normal_distribution<> flops(average_flops, sigma_flops);
  std::normal_distribution<> mem(average_memory, sigma_memory);
  std::normal_distribution<> insize(average_infile_size, sigma_infile_size);

  for (size_t j = 0; j < num_jobs; j++) {
    // Sample strictly positive task flops
    double dflops = flops(gen);
    while (dflops < 0.) dflops = flops(gen);
    // Sample strictly positive task memory requirements
    double dmem = mem(gen);
    while (dmem < 0.) dmem = mem(gen);

    // Connect the chains spanning all input-files of a job
    wrench::WorkflowTask* endtask = nullptr;
    wrench::WorkflowTask* enddummytask = nullptr;
    for (size_t f = 0; f < infiles_per_task; f++) {
      // Sample inputfile sizes
      double dinsize = insize(gen);
      while (dinsize < 0.) dinsize = insize(gen); 
      // when blockstreaming is turned off
      if (!use_blockstreaming) {
        xrd_block_size = dinsize;
      }  
      // Chunk inputfiles into blocks and create blockwise tasks and dummy tasks
      // chain them as sketched in https://github.com/HerrHorizontal/DistCacheSim/blob/test/sgbatch/Sketches/Task_streaming_idea.pdf to enable task streaming
      size_t nblocks = static_cast<size_t>(dinsize/xrd_block_size);
      wrench::WorkflowTask* dummytask_parent = nullptr;
      wrench::WorkflowTask* task_parent = nullptr;
      if (enddummytask && endtask) {
        // Connect the chain to the previous input-file's
        //TODO: test if both exist simultaneously
        dummytask_parent = enddummytask;
        task_parent = endtask;
      }
      for (size_t b = 0; b < nblocks; b++) {
        // Dummytask with inputblock and previous dummytask dependence
        // with minimal number of memory and flops 
        auto dummytask = workflow->addTask("dummytask_"+std::to_string(j)+"_file_"+std::to_string(f)+"_block_"+std::to_string(b), dummy_flops, 1, 1, dummy_flops);
        double blocksize = xrd_block_size;
        dummytask->addInputFile(workflow->addFile("infile_"+std::to_string(j)+"_file_"+std::to_string(f)+"_block_"+std::to_string(b), blocksize));
        if (dummytask_parent) {
          workflow->addControlDependency(dummytask_parent, dummytask);
        }
        dummytask_parent = dummytask;
        // Task with dummytask and previous task dependence
        double blockflops = dflops * blocksize/dinsize;
        auto task = workflow->addTask("task_"+std::to_string(j)+"_file_"+std::to_string(f)+"_block_"+std::to_string(b), blockflops, 1, 1, dmem);
        workflow->addControlDependency(dummytask, task);
        if (task_parent) {
          workflow->addControlDependency(task_parent, task);
        }
        task_parent = task;
        // Last blocktask is endtask
        if (b == nblocks-1) {
          enddummytask = dummytask;
          endtask = task;
        }
      }
      // when the input-file size is not an integer multiple of the XRootD blocksize create a last block task which takes care of the modulo
      // when blockwise streaming is turned off this evaluates to false
      if (double blocksize = (dinsize - nblocks*xrd_block_size)) {
        auto dummytask = workflow->addTask("dummytask_"+std::to_string(j)+"_file_"+std::to_string(f)+"_block_"+std::to_string(nblocks), dummy_flops, 1, 1, dummy_flops);
        dummytask->addInputFile(workflow->addFile("infile_"+std::to_string(j)+"_file_"+std::to_string(f)+"_block_"+std::to_string(nblocks), blocksize));
        if (dummytask_parent) {
          workflow->addControlDependency(dummytask_parent, dummytask);
        }
        double blockflops = dflops * blocksize/dinsize;
        auto task = workflow->addTask("task_"+std::to_string(j)+"_file_"+std::to_string(f)+"_block_"+std::to_string(nblocks), blockflops, 1, 1, dmem);
        workflow->addControlDependency(dummytask, task);
        if (task_parent) {
          workflow->addControlDependency(task_parent, task);
        }
        enddummytask = dummytask;
        endtask = task;
      }
    }
    endtask->addOutputFile(workflow->addFile("outfile_"+std::to_string(j), 0.0));
    //TODO: test if the complete chain has the right amount of tasks and dummytasks
  }
}


int main(int argc, char **argv) {

  // Declaration of the top-level WRENCH simulation object
  auto simulation = new wrench::Simulation();

  // Initialization of the simulation
  simulation->init(&argc, argv);


  /* Parsing of the command-line arguments for this WRENCH simulation */
  if (argc != 6) {
    std::cerr << "Usage: " << argv[0];
    std::cerr << " <xml platform file> <number of jobs> <input files per job> <average inputfile size> <cache hitrate>";
    std::cerr << " [--wrench-full-log || --log=custom_wms.threshold=info]";
    std::cerr << std::endl;
    exit(1);
  }

  // The first argument is the platform description file, written in XML following the SimGrid-defined DTD
  char *platform_file = argv[1];
  // The second argument is the number of jobs which need to be executed
  size_t num_jobs = arg_to_sizet(argv[2]);
  // The third argument is the number of input files per job which need to be transferred
  size_t infiles_per_job = arg_to_sizet(argv[3]);
  // The fourth argument is the average size of the inputfiles in bytes
  double average_infile_size = arg_to_double(argv[4]);
  // The fifth argument is the fractional cache hitrate
  double hitrate = arg_to_double(argv[5]);

  // Turn on/off blockwise streaming of input-files
  //TODO: add CLI features for the blockwise streaming flag
  bool use_blockstreaming = true; // ! turned off for test purposes


  /* Create a workflow */
  std::cerr << "Loading workflow..." << std::endl;
  auto workflow = new wrench::Workflow();

  // Sample task parameters from normal distributions
  // Set normal distribution parameters
  double average_flops = 1.2*1000*1000;
  double average_memory = 2*1000*1000*1000;
  double sigma_flops = 0.5*average_flops;
  double sigma_memory = 0.5*average_memory;
  double sigma_infile_size = 0.5*average_infile_size;
  
  fill_streaming_workflow(
    workflow, 
    num_jobs, infiles_per_job,
    average_flops, sigma_flops,
    average_memory,sigma_memory,
    average_infile_size, sigma_infile_size,
    use_blockstreaming
  );

  std::cerr << "The workflow has " << workflow->getNumberOfTasks() << " tasks in " << std::to_string(num_jobs) << " chains" << std::endl;


  /* Read and parse the platform description file to instantiate a simulation platform */
  std::cerr << "Instantiating SimGrid platform..." << std::endl;
  simulation->instantiatePlatform(platform_file);


  /* Create storage and compute services and add them to the simulation */ 
  // Loop over vector of all the hosts in the simulated platform
  std::vector<std::string> hostname_list = simulation->getHostnameList();
  // Create a list of storage services that will be used by the WMS
  std::set<std::shared_ptr<wrench::StorageService>> storage_services;
  // Split into cache storages
  std::set<std::shared_ptr<wrench::StorageService>> cache_storage_services;
  // and a remote storage that is able to serve all file requests
  std::set<std::shared_ptr<wrench::StorageService>> remote_storage_services;
  // Create a list of compute services that will be used by the HTCondorService
  std::set<std::shared_ptr<wrench::ComputeService>> condor_compute_resources;
  std::string wms_host = "WMSHost";
  for (std::vector<std::string>::iterator hostname = hostname_list.begin(); hostname != hostname_list.end(); ++hostname) {
    std::string hostname_transformed = *hostname;
    std::for_each(hostname_transformed.begin(), hostname_transformed.end(), [](char& c){c = std::tolower(c);});
    // Instantiate storage services
    // WMSHost doesn't need a StorageService
    if (*hostname != wms_host) {
      std::string storage_host = *hostname;
      std::cerr << "Instantiating a SimpleStorageService on " << storage_host << "..." << std::endl;
      auto storage_service = simulation->add(new wrench::SimpleStorageService(storage_host, {"/"}));
      if (hostname_transformed.find("remote") != std::string::npos) {
        remote_storage_services.insert(storage_service);
      } else {
        cache_storage_services.insert(storage_service);
      }
      storage_services.insert(storage_service);
    } 
    // Instantiate bare-metal compute-services
    if (
      (*hostname != wms_host) && 
      (hostname_transformed.find("storage") == std::string::npos)
    ) {
      condor_compute_resources.insert(
        simulation->add(
          new wrench::BareMetalComputeService(
            *hostname,
            {std::make_pair(
              *hostname,
              std::make_tuple(
                wrench::Simulation::getHostNumCores(*hostname),
                wrench::Simulation::getHostMemoryCapacity(*hostname)
              )
            )},
            ""
          )
        )
      );
    }
  }
  // Instantiate a HTcondorComputeService and add it to the simulation
  std::set<shared_ptr<wrench::ComputeService>> htcondor_compute_services;
  htcondor_compute_services.insert(shared_ptr<wrench::ComputeService>(simulation->add(
    new wrench::HTCondorComputeService(
      wms_host,
      condor_compute_resources,
      {
        {wrench::HTCondorComputeServiceProperty::NEGOTIATOR_OVERHEAD, "1.0"},
        {wrench::HTCondorComputeServiceProperty::GRID_PRE_EXECUTION_DELAY, "10.0"},
        {wrench::HTCondorComputeServiceProperty::GRID_POST_EXECUTION_DELAY, "10.0"},
        {wrench::HTCondorComputeServiceProperty::NON_GRID_PRE_EXECUTION_DELAY, "5.0"},
        {wrench::HTCondorComputeServiceProperty::NON_GRID_POST_EXECUTION_DELAY, "5.0"}
      },
      {}
    )
  )));


  /* Instantiate a file registry service */
  std::string file_registry_service_host = wms_host;
  std::cerr << "Instantiating a FileRegistryService on " << file_registry_service_host << "..." << std::endl;
  auto file_registry_service =
          simulation->add(new wrench::FileRegistryService(file_registry_service_host));


  // /* Instantiate a network proximity service */
  // std::string network_proximity_service_host = wms_host;
  // std::cerr << "Instantiating a NetworkProximityService on " << network_proximity_service_host << "..." << std::endl;
  // auto network_proximity_service =
  //         simulation->add(new wrench::NetworkProximityService(
  //           network_proximity_service_host, 
  //           hostname_list, 
  //           {}, 
  //           {}
  //         ));


  /* Instantiate a WMS */
  auto wms = simulation->add(
          new SimpleWMS(
            htcondor_compute_services, 
            storage_services,
            {},//{network_proximity_service},
            file_registry_service, 
            wms_host,
            hitrate
          )
  );
  wms->addWorkflow(workflow);


  /* Instatiate inputfiles */
  // Check that the right remote_storage_service is passed for initial inputfile storage
  // TODO: generalize to arbitrary numbers of remote storages
  if (remote_storage_services.size() != 1) {
    throw std::runtime_error("This example Simple Simulator requires a single remote_storage_service");
  }
  auto remote_storage_service = *remote_storage_services.begin();

  // It is necessary to store, or "stage", input files (blocks)
  std::cerr << "Staging input files..." << std::endl;
  std::vector<wrench::WorkflowTask*> tasks = workflow->getTasks();
  try {
    for (auto task : tasks) {
      auto input_files = task->getInputFiles();
      // Shuffle the input files
      std::shuffle(input_files.begin(), input_files.end(), gen);
      // Compute the task's incremental inputfiles size
      double incr_inputfile_size = 0.;
      for (auto f : input_files) {
        incr_inputfile_size += f->getSize();
      }
      // Distribute the infiles on all caches untill desired hitrate is reached
      double cached_files_size = 0.;
      for (auto const &f : input_files) {
        simulation->stageFile(f, remote_storage_service);
        if (cached_files_size <= hitrate*incr_inputfile_size) {
          for (auto cache : cache_storage_services) {
            simulation->stageFile(f, cache);
          }
          cached_files_size += f->getSize();
        }
      }
    }
  } catch (std::runtime_error &e) {
      std::cerr << "Exception: " << e.what() << std::endl;
      return 0;
  }

  simulation->getOutput().enableDiskTimestamps(true);
  simulation->getOutput().enableFileReadWriteCopyTimestamps(true);
  simulation->getOutput().enableWorkflowTaskTimestamps(true);

  /* Launch the simulation */
  std::cerr << "Launching the Simulation..." << std::endl;
  try {
    simulation->launch();
  } catch (std::runtime_error &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 0;
  }
  std::cerr << "Simulation done!" << std::endl;


  /* Analyse event traces */
  auto simulation_output = simulation->getOutput();
  auto trace = simulation_output.getTrace<wrench::SimulationTimestampTaskCompletion>();
  for (auto const &item : trace) {
    std::cerr << "Task " << item->getContent()->getTask()->getID() << " completed at time " << item->getDate() << std::endl;
  }
  // and dump JSONs containing the generated data
  std::cerr << "Dumping generated data..." << std::endl;

    bool include_platform = false;
    bool include_workflow_exec = true;
    bool include_workflow_graph = false;
    bool include_energy = false;
    bool generate_host_utilization_layout = false;
    bool include_disk = true;
    bool include_bandwidth = false;
  simulation_output.dumpUnifiedJSON(workflow, "/tmp/unified.json", include_platform, include_workflow_exec, include_workflow_graph, include_energy, generate_host_utilization_layout, include_disk, include_bandwidth);
//  simulation_output.dumpDiskOperationsJSON("tmp/diskOps.json", true);
//  simulation_output.dumpLinkUsageJSON("tmp/linkUsage.json", true);
//  simulation_output.dumpPlatformGraphJSON("tmp/platformGraph.json", true);
//  simulation_output.dumpWorkflowExecutionJSON(workflow, "tmp/workflowExecution.json", false, true);
  // and the workflow graph
//  simulation_output.dumpWorkflowGraphJSON(workflow, "tmp/workflowGraph.json", true);


  return 0;
}

