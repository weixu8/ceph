===============
 RADOS Gateway
===============

RADOS Gateway is an object storage interface built on top of ``librados`` to
provide applications with a RESTful gateway to RADOS clusters. The RADOS Gateway
supports two interfaces:

#. **S3-compatible:** Provides block storage functionality with an interface that 
   is compatible with a large subset of the Amazon S3 RESTful API.

#. **Swift-compatible:** Provides block storage functionality with an interface
   that is compatible with a large subset of the OpenStack Swift API.

RADOS Gateway is a FastCGI module for interacting with ``librados``. Since it
provides interfaces compatible with OpenStack Swift and Amazon S3, RADOS Gateway
has its own user management. RADOS Gateway can store data in the same RADOS
cluster used to store data from Ceph FS clients or RADOS block devices.
Each interface (S3 or Swift) provides its own namespace.

.. toctree::
	:maxdepth: 1

	Manual Install <manual-install>
	Configuration <config>
	Config Reference <config-ref>
	S3 API <s3>
	Swift API <swift/index>
	Admin API <admin/index>
