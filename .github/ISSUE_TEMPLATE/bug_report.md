---
name: Bug report
about: Create a report to help us improve BeeGFS
title: ''
labels: bug, new
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is, which component (mgmtd, meta, storage, client, others or any combination) is affected and what effects (error messages, performance degradation, crashes, ...) can be observed.

**Describe the system**
A short description of the system BeeGFS is running on that covers all information relevant to the issue. For example (but not limited to):
1. Number and type of BeeGFS services and their distribution across physical machines
2. Information about the network interconnect (technology, protocols, topology)
3. Information about underlying file systems and storage hardware
4. Operating system and proprietary driver information (versions, RDMA drivers)
5. Software that is accessing BeeGFS
 
**To Reproduce**
Steps to reproduce the behavior:
1. Set configuration option "x" to "y" for component "c"
2. Start component "c" on machine "m"
3. Run application "a" with parameters "p"
4. Observe behavior "b"

**Expected behavior**
A clear and concise description of what you expected to happen.

**Log messages, error outputs**
If available, add relevant log files (anonymize if necessary), error messages, `perf` measurements, `strace` outputs, core dumps and everything else you have collected and can share publicly.

**Additional context**
Add any other context not covered above.
