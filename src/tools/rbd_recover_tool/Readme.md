## rbd-recover-tool

Welcome to the basic documentation of Ceph's RBD desaster recovery helper
**Bold** rbd-recover-tool


### Goal

ceph **rbd-recover-tool** may be useful for the unlucky ones, seeing themself in a situation
where your ceph-cluster isn't functional and/or accessible through regular administration tasks.
You may realize, that you can't repair the culster infrastructure to get down to your data
**and** your backups aren't handy or do not reflact your expected point in time data status.

Now, let us predict that the used hardware components storing the **data objects**

- aren't physical damaged
- are available and handy 
- can be made accessible on a local system (mounted disks on an admin-host)
- can be made accessible on a remote system (mounted disks on an osd-host)
- you can reach the remote hosts via ssh

You may be able to recover the rbd-objects with **rbd-recover-tool**.
You will need an OS (eg. Linux) running the ceph stack, enriched with the listed
[helper tools](https://github.com/rzerres/ceph/edit/master/recover-tools.md). 


### Architecture

**rbd-recover-tool** works in a client-server model and has two components:

- one admin-node
- any number of osd-nodes

Configuration files on the **admin node** will hold the required information where
rbd-data can be reached beside the infomation, where recovered data should be stored.

This configuration-data is parsed and analized through a master control scipt **rbd-recover-tool**. It offers run-type modes and let you select what to do.

#### Runtime modes

1. The **setup-routine**

This mode is the initial task and need to be run once. It will take care to

- prepare needed data-structures on the admin-node
- prepare needed data-structures on each involved osd-node
- test the availability of needed recovery-tools
- create the config file 'osd_host' representing all involved osd-nodes
- populate control scripts on each involved osd-node

2. The **database-routine**

After preparation, it's time to analyse your ceph-data storage.
Linear to the size of your managed cluster data, this routine will consume some time.
Basically it will

- gather placement-group informations (on all osd-nodes)
- gather object infos and metadata (on all osd-nodes)
- decrypt the object epochs (on all osd-nodes)

The gained data will be stored in subfolders underneath the root-path on the admin-node.

3. The **list-routine**

**rbd-recover-tool** will collect all **object-names** of recognizeable rbd-objects (v1 format / v2 format) and save them in a list for further tasks.

4. The **lookup-routine**

Now it's time to decrpyt the DBObject-Structure of your selectable rbd-objects. You need to
define the object with an uri given its ceph-notation:

* <pool-id>/<image-name>[@snapshot]

**rbd-recover-tool** will try its best, to present you the decrypted metadata

- rbd-id
- image fomat
- image size
- image snaphost-information
- object order
- object features
- object snap-sequence


5. The **recover-routine**

To recover a rbd-object you need to pass its uri given its ceph-notation:

* <pool-id>/<image-name>[@snapshot]

ceph rbd-recover-tool is just used for this! It collects all objects associated to this rbd-id
from involved osd-nodes (selecting the latest pg epoch). Then the objects stitche to a complete
complete image, respecting their offset.

#### Config files

Per default, **rbd-recover-tool** will search its config-files in a subdirectory
called 'config'

1. osd_host_path

This file will take any number of lines holding the involved osd nodes.
You need to declare their hostname and the path, where the osd-data is mounted.
Hint: the admin-node can function as an osd-node as well

```~rbd-restore-tool/config/osd_host_path
  osdhost0	/var/lib/ceph/osd/ceph-0
  osdhost1	/var/lib/ceph/osd/ceph-1
  ......
`

2. mon_host

This file will take any number of lines holding the involved monitor nodes.

```~rbd-restore-tool/config/mon-host
  monhost0
  monhost1
  ......

3. mds_host

This file will take any number of lines holding the involved metadata-server nodes.

```~rbd-restore-tool/config/mds-host
  mdshost0
  mdshost1
  ......

#### Ascii-Art

                      +---- osd.0
                      |
admin_node -----------+---- osd.1
                      |
                      +---- osd.2
		      |
                      ......

#### Debugging

If you encounter errors or operation fails while running a **rbd-recover-tool** mode
you can enable the debug/verbose option which will pesent hopefully useful infomation
to nail down the problem.

Any function can be called directly (on admin-node or osd-node) providing the needed
parameters.

##### osd-node

Call the osd-node control-script  (default: /var/rbd_tool/osd_job)

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

This tool was invented 2014 by Min chen (michen@ubuntukylin.com), supporting the Firefly release (ceph-0.80.x).

Since 2 years it hasen't seen any updates. Driven by a desaster data-loss szenario
at a customer site in late October 2016, I have started to patch the code-base
to integrate with Ceph's Jewel release (ceph-10.2.x).


### Copyright

**rbd-recover-tool** is free software.

This page is  written in Markdown syntax and is located at [editor on GitHub](https://github.com/rzerres/ceph/edit/master/Readme.md).
