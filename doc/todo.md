## Extract Faust graph & params UI to separate `FaustImGui` library repo

Going to use this for [Mesh2Audio project](https://github.com/GATech-CSE-6730-Spring-2023-Project/mesh2audio).

Done when both FlowGrid and Mesh2Audio both render faust graph & params UI using this new lib.

## Audio invariants & current audio graph connection matrix issues

* When audio device is not running, but Faust code is valid, all visual Faust elements should be present (graph, params, code).
* Whenever audio device is running and Faust code is valid, Faust should reflect the current device sample rate.
* Num rows & cols of audio connection matrix always reflect the number of output/input busses respectively

### Problems with current matrix connections representation:

* disabling and reenabling a node will not remember its previous connections
* changing IO devices will not remember connections

### Solutions

* keep list of all seen node source/dest names in preferences, ordered by node init time
  - instead of storing connections as a matrix, store as a map from source node index to list of enabled dest node indices
    - only save source nodes with at least one dest node enabled
  - render all enabled dest-rows/source-cols in connection matrix
  - when disabling a node (removing it from the graph), connection state doesn't change - only the bool enabled state
  - no more `SetMatrix` action. Use `struct SetConnectionEnabled { Count source_node_id; Count dest_node_id;, bool enabled; }`, where node IDs are corresponding index into preferences source/dest node name list.
    - during initialization, issue a `SetConnectionEnabled` for each default connection.
