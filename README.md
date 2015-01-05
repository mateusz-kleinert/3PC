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
  --coord_timeout_q
  --coord_timeout_w
  --coord_timeout_p
  --coord_failure_q
  --coord_failure_w
  --coord_failure_p
  --cohort_member_timeout_q
  --cohort_member_timeout_w
  --cohort_member_timeout_p
  --cohort_member_failure_q
  --cohort_member_failure_w
  --cohort_member_failure_p
  --cohort_member_abort_q

`coord` stands for coordinator node - node with id 0
`cohort_member` stands for any other node in the network
`q`, `w` and `p` stands for algorithm states in which transaction abort, timeout or fail 