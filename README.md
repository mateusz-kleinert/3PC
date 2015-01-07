Three-phase Commit
==================

Three-phase Commit algorithm implemented in C++ and MPI.


How to compile and run:
-----------------------
```bash
mpic++ main.cpp -std=c++11
mpirun -n [proc_num] ./a.out [options]
```

Options for simulation:
 - --t_intersection
 - --coord_timeout_q
 - --coord_timeout_w
 - --coord_timeout_p
 - --coord_failure_q
 - --coord_failure_w
 - --coord_failure_p
 - --cohort_member_timeout_q
 - --cohort_member_timeout_w
 - --cohort_member_timeout_p
 - --cohort_member_failure_q
 - --cohort_member_failure_w
 - --cohort_member_failure_p
 - --cohort_member_abort_q

This code simulates commit of two simultaneous transactions. The cohort sets are separate. To change this setting use
`t_intersection` switch, then one node of the network will be shared.  

`coord` stands for coordinator node - as coordinator there are only two nodes: 0 and 1. However, timeout and failure can
be simulated only on the first coordinator.

`cohort_member` stands for any other node in the network. Simulation of timeout and failure of cohort member affects random nodes. 

`q`, `w` and `p` stands for algorithm states in which transaction abort, timeout or fail 

According to states described [here](http://courses.cs.vt.edu/~cs5204/fall00/distributedDBMS/sreenu/3pc.html).
