## rbd-recover-tool

Welcome to the basic documentation of Ceph's RBD desaster recovery helper
**rbd-recover-tool**


### Helper Tools

ceph **rbd-recover-tool** makes use of other ceph helper utilities to realise its goal
to recover rbd objects.

These tools are esentil for the intended behavior and need to be available on all
involved nodes:

	- awk
	- bash
	- basename
	- cat
	- ceph-kvstore-tool
	- find
	- grep
	- hostname
	- ps
	- xxd
	- ssh

