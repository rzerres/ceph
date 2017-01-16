## rbd-recover-tool

Welcome to the basic documentation of Ceph's RBD desaster recovery helper
**rbd-recover-tool**


### Goal

ceph **rbd-recover-tool** may be useful for the unlucky ones, seeing themself in a situation,
where your ceph-cluster isn't functional. Either regular administration tasks aren't
accessible any more, our you hardware did crash.
You may realize, that you can't repair ceph's culster infrastructure to get down to your data
**and** your backups aren't handy. Sure you have backups, but they may not reflect the
expected point in time data status.

Let us predict, that the used hardware components storing the **data objects**
(your osd-disks)

- aren't physical damaged
- are available and handy 
- can be made accessible on new hardware (mounted disks on your admin-host)
- can be made accessible on a remote system (mounted disks on an osd-host)
- you can reach the remote systems via ssh

In this case, you may be able to recover rbd-objects with **rbd-recover-tool**.
Install rbd-recover-tool on your admin-host (eg. Linux), combined with the listed
[helper tools](https://github.com/rzerres/ceph/edit/master/recover-tools.md). 


### Architecture

**rbd-recover-tool** works in a client-server model with following components:

- one admin-node
- any number of osd-nodes

Its operation will be controlled via the **admin node**. Config files will
hold the required information, how the ceph data can be reached, and where
where recoverable data should be stored.

This configuration-data is parsed and analized through a master control scipt **rbd-recover-tool**.
It offers run-type modes and let you select what to do.

#### Runtime modes

1. The **setup-routine**

	This mode is the initial task and need to be run once. It will take care to

	- prepare the recovery data-structures on the admin-node
	- prepare the recovery data-structures on each involved osd-node
	- test the availability of needed tools
	- construct the config file 'osd_host' (representing the list of involved osd-nodes)
	- populate control scripts to each involved osd-node

2. The **database-routine**

	After preparation, it's time to analyse your ceph-data storage. 
	Linear to the size of your managed cluster data, this routine will consume some time. Basically it will
	
	scatter jobs on each osd to process

	- reference-objects (find rbd-patterns)
	- object-map informations (omap_lists)
	- placement-groups (pgid_list)
	- epochs for the reference-objects (node_pg_epoch)

	single-node: gather processed data on the osd nodes

	- create subdirs for each osd
	- collect image lists (v1/v2 objects)
	
	rbd-metadata: transfer collected data to the admin-node

	- reference object-file per osd-node (database)
	- placement-group collection  (pg_coll)
	- image header refrence-file  (image_coll_v1)
	- image header refrence-file  (image_coll_v2)
	
	All processes will run in parallel if possible.

3. The **list-routine**

	You can list the recognized **objet-names** for further processing.
	**rbd-recover-tool** will separate the names respecting their format (v1 and v2 objects).

4. The **lookup-routine**

	Time to analyze the DBObject-Structure for the collected rbd-objects.
	Start the lookup process for the object in question, which is labeled via the following ceph-notation:

	* \<pool-id\>/\<image-name\>[@snapshot]"

	**rbd-recover-tool** will try its best, to present you the objects decrypted metadata

	- rbd-id
	- image fomat
	- image size
	- object number
	- object order
	- object features
	- object snaphost-information
	- object striping-information

5. The **recover-routine**

	To recover a rbd-object you need to pass its uri given its ceph-notation:

	* \<pool-id\>/\<image-name\>[@snapshot]" \<dest-dir\>

	**rbd-recover-tool** is just used for this! It collects all objects associated to this rbd-id
	found in object stores on the involved osd-nodes. **rbd-recover-tool** will use the latest pg epoch.
	Then the objects are stitched together forming an importable/mountable raw-image.

#### Config files

Per default, **rbd-recover-tool** will search its config-files in a subdirectory
called 'config'

1. osd_host_path

This file will take any number of lines holding the involved osd nodes.
You need to declare their hostname and the path, where the osd-data is mounted.
Hint: the admin-node can function as an osd-node as well

	~rbd-restore-tool/config/osd_host_path
	osdhost0	/var/lib/ceph/osd/ceph-0
	osdhost1	/var/lib/ceph/osd/ceph-1
	......

2. mon_host

This file will take any number of lines holding the involved monitor nodes.

	~rbd-restore-tool/config/mon-host
	monhost0
	monhost1
	......
  
3. mds_host

This file will take any number of lines holding the involved metadata-server nodes.

	~rbd-restore-tool/config/mds-host
	mdshost0
	mdshost1
	......

#### Ascii-Art

						+---- osd.0
						|
	admin_node ---------+---- osd.1
						|
						+---- osd.3
						|
						......

#### Debugging

If you encounter errors or an operation fails while running a **rbd-recover-tool** mode,
you can enable the debug/verbose option. Hopefully this will pesent useful infomation
to nail down the problem.

Any function can be called directly (on admin-node or osd-node) providing the needed
parameters.

##### osd-node

Call the osd-node control-script  (default: /var/rbd_recover_tool/osd_job)

	./osd_job <operation>
	<opeartion> :
	do_image_id <image_id_hobject>		        # get rbd-id for rbd-object in v2 format
	do_image_id <image_header_hobject>	        # get rbd-id for rbd-object in v1 format
	do_image_metadata_v1 <image_header_hobject>  	# get object metadata for rbd-object in v1 format
	do_image_metadata_v2 <image_header_hobject>  	# get object metadata for rbd-object in v2 format
	                                                  maybe pg_epoch is not latest
	do_image_list 				        # get all rbd-objects recognized on this osd
	do_pg_epoch				        # collect pg_epoch information for given osd
							# default storage: /var/rbd_tool/single_node/node_pg_epoch
	do_omap_list    			        # list all omap headers and omap entries on this osd

### FAQ

The FAQ lists will summarize confusing test cases ...


### History

This tool was invented 2014 by Min Chen (michen@ubuntukylin.com), supporting the Firefly release (ceph-0.80.x).
It was extended in 2017 by Ralf Zerres (ralf.zerres@networkx.de), supporting the Jewel release (ceph-10.2.x).

The code is a collection of bash scripts. Since it initial development, it had not seen any updates after 2014.
Driven by a desaster data-loss szenario at a customer site in late October 2016, patching the code-base was 
started to integrate with Ceph's Jewel release. By that time, there was no tool available, that could extract
stripe_units/stripe_counts for rbd-objects stored on a corrupt cluster.


### Copyright

Copyright (C) 2017 Ralf Zerres

Author:   Min Chen <minchen@ubuntukylin.com>
Extended: Ralf Zerres <ralf.zerres@networkx.de>

**rbd-recover-tool** and its documentation is free software. you can redistribute it and/or modify
it under the terms of the GNU Library Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

This page is  written in Markdown syntax and is located at [editor on GitHub](https://github.com/rzerres/ceph/edit/master/README.md).
