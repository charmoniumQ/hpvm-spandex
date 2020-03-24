# HPVM-C Language Specification
An HPVM program is a combination of host code and one or more data flow graphs (DFG) at the IR level. We provide C function declarations representing the HPVM intrinsics that allow creating, querying, and interacting with the DFGs. More details about the HPVM IR intrinsics can be found in [the HPVM IR Specification.](/hpvm/docs/hpvm-specification.md).

An HPVM-C program contains both the host and the DFG code. Each HPVM kernel, represented by a leaf node in the DFG, can be compiled to multiple different targets (e.g. CPU and GPU) as described below. 

This document describes all the API calls that can be used in an HPVM-C program.

## Host API

```void __hpvm__init()```  
Used before all other HPVM calls to initialize the HPVM runtime.

```void __hpvm__cleanup()```  
Used at the end of HPVM program to clean up all remaining runtime-created HPVM objects.

```void llvm_hpvm_track_mem(void* ptr, size_t sz)```  
Insert memory starting at ```ptr``` of size ```sz``` in the memory tracker of HPVM runtime.

```void llvm_hpvm_untrack_mem(void* ptr)```  
Stop tracking the memory object identified by ```ptr```.

```void llvm_hpvm_request_mem(void* ptr, size_t sz)```  
If the memory object identified by ```ptr``` is not in host memory, copy it to host memory.

```void* __hpvm__launch(unsigned isStream, void* rootGraph, void* args)```  
Launches the execution of the dataflow graph with node function ```rootGraph```. ```args``` is a pointer to a packed struct, containing one field per argument of the RootGraph function, consecutively. For non-streaming DFGs with a non empty result type, ```args``` must contain an additional field of the type ```RootGraph.returnTy```, where the result of the graph will be returned. ```isStream``` chooses between a non streaming (0) or streaming (1) graph execution. Returns a handle to the executing graph.

```void __hpvm__wait(void* G)```  
Waits for completion of execution of the dataflow graph with handle ```G```.

```void __hpvm__push(void* G, void* args)```  
Push set of input data items, ```args```, (same as type included in launch) to streaming DFG with handle ```G```.

```void* __hpvm__pop(void* G)```  
Pop and return data produced from one execution of streaming DFG with handle ```G```. The return type is a struct containing a field for every output of DFG. 

## Internal Node API

```void* __hpvm__createNodeND(unsigned dims, void* F, ...)```  
Creates a static dataflow node replicated in ```dims``` dimensions (0 to 3), each executing node function ```F```. The arguments following ```F``` are the size of each dimension, respectively, passed in as a ```size_t```. Returns a handle to the created dataflow node.

```void* __hpvm__edge(void* src, void* dst, unsigned replType, unsigned sp, unsigned dp, unsigned isStream)```  
Creates an edge from output ```sp``` of node ```src``` to input ```dp``` of node ```dst```. If ```replType``` is 0, the edge is a one-to-one edge, otherwise it is an all-to-all edge. ```isStream``` defines whether or not the edge is streaming. Returns a handle to the created edge.

```void __hpvm__bindIn(void* N, unsigned ip, unsigned ic, unsigned isStream)```  
Binds the input ```ip``` of the current node to input ```ic``` of child node function ```N```. ```isStream``` defines whether or not the input bind is streaming.

```void __hpvm__bindOut(void* N, unsigned op, unsigned oc, unsigned isStream)```  
Binds the output ```op``` of the current node to output ```oc``` of child node function ```N```. ```isStream``` defines whether or not the output bind is streaming.

```void __hpvm__hint(enum Target target)``` (C\)  
```void __hpvm__hint(hpvm::Target target)``` (C++)  
Must be called once in each node function. Indicates which hardware target the current function should run in.

```void __hpvm__attributes(unsigned ni, …, unsigned no, …)```  
Must be called once at the beginning of each node function. Defines the properties of the pointer arguments to the current function. ```ni``` represents the number of input arguments, and ```no``` the number of output arguments. The arguments following ```ni``` are the input arguments, and the arguments following ```no``` are the output arguments. Arguments can be marked as both input and output. All pointer arguments must be included.

## Leaf Node API
```void __hpvm__hint(enum Target target)``` (C\)  
```void __hpvm__hint(hpvm::Target target)``` (C++)  
As described in internal node API.

```void __hpvm__attributes(unsigned ni, …, unsigned no, …)```  
As described in internal node API.

```void __hpvm__return(unsigned n, ...)```  
Returns ```n``` values from a leaf node function. The remaining arguments are the values to be returned. All ```__hpvm__return``` statements within the same function must return the same number of values.

```void* __hpvm__getNode()```  
Returns a handle to the current leaf node.

```void* __hpvm__getParentNode(void* N)```  
Returns a handle to the parent node of node ```N```.

```long __hpvm__getNodeInstanceID_{x,y,z}(void* N)```  
Returns the dynamic ID of the current instance of node ```N``` in the x, y, or z dimension respectively. The dimension must be one of the dimensions in which the node is replicated.

```long __hpvm__getNumNodeInstances_{x,y,z}(void* N)```  
Returns the number of dynamic instances of node ```N``` in the x, y, or z dimension respectively. The dimension must be one of the dimensions in which the node is replicated.

```void* __hpvm__malloc(long nBytes)```  
Allocate a block of memory of size ```nBytes``` and returns a pointer to it. The allocated object can be shared by all nodes. *Note that the returned pointer must somehow be communicated explicitly for use by other nodes.*

```int __hpvm__atomic_add(int* m, int v)```  
Atomically adds ```v``` to the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_sub(int* m, int v)```  
Atomically subtracts ```v``` from the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_min(int* m, int v)```  
Atomically computes the min of ```v``` and the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_max(int* m, int v)```  
Atomically computes the max of ```v``` and the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_xchg(int* m, int v)```  
Atomically swaps ```v``` with the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_and(int* m, int v)```  
Atomically computes the bitwise AND of ```v``` and the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_or(int* m, int v)```  
Atomically computes the bitwise OR of ```v``` and the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```int __hpvm__atomic_xor(int* m, int v)```  
Atomically computes the bitwise XOR of ```v``` and the value stored at memory location ```[m]``` w.r.t. the dynamic instances of the current leaf node and stores the result back into ```[m]```. Returns the value previously stored at ```[m]```.

```void __hpvm__barrier()```  
Local synchronization barrier across dynamic instances of current leaf node.

# Porting a Program from C to HPVM-C

The following represents the required steps to port a regular C program into an HPVM program with HPVM-C. These steps are described at a high level; for more detail, please see [hpvm-cava](/hpvm/test/benchmarks/hpvm-cava) provided in [benchmarks](/hpvm/test/benchmarks).
* Separate the computation that will become a kernel into its own (leaf node) function and add the attributes and target hint.
* Create a level 1 wrapper node function that will describe the thread-level parallelism (for the GPU). The node will:
    * Use the ```createNode[ND]()``` method to create a kernel node and specify how many threads will execute it.
    * Bind its arguments to the kernel arguments.
* If desired, create a level 2 wrapper node function which will describe the threadblock-level parallalism (for the GPU). This node will:
    * Use the ```createNode[ND]()``` method to create a level 1 wrapper node and specify how many threadblocks will execute it.
    * Bind its arguments to its child node's arguments.
* A root node function that creates all the top-level wrapper nodes, binds their arguments, and connects their edges.
    * Each root node represents a DFG.
* All the above node functions have the combined arguments of all the kernels that are nested at each level. 
* The host code will have to include the following:
    * Initialize the HPVM runtime using the ```init()``` method.
    * Create an argument struct for each DFG and assign its member variables.
    * Add all the memory that is required by the kernel into the memory tracker.
    * Launch the DFG by calling the ```launch()``` method on the root node function, and passing the corresponding argument struct.
    * Wait for the DFG to complete execution.
    * Read out any generated memory using the ```request_mem()``` method.
    * Remove all the tracked memory from the memory tracker.
