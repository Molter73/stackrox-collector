# Collector Ansible Tools

The files contained within this directory are [ansible](https://www.ansible.com/resources/get-started)
roles and playbooks written to simplify VM life-cycle control, and execution of testing, both
during development and on CI systems[^1].

## Prerequisites

In order to run these playbooks you must have a python version >=3.9.
To install ansible and the necessary dependencies, simply run:

```
$ pip3 install -r requirements.txt
```

On a Mac, the default python3 is 3.7, so use brew to install the latest ansible:

```
$ brew install ansible
$ pip3 install -r requirements.txt
```

To manage IBM Z and Power VMs through IBM Cloud, you will also need to download and install the following ansible collection:
```
$ ansible-galaxy collection install ibm.cloudcollection
```

## Image builds
### Builder image

The `ci-build-builder.yml` playbook is meant to be used by CI, it handles the
build process for the builder image, as well as retagging and pushing to
quay.io.

#### Environment variables used by the playbook

| Name | Description |
| ---  | ---         |
| COLLECTOR_BUILDER_TAG | The tag used for the builder image. |
| PLATFORM | The platform the images are being built for, currently the supported values are:<br>- linux/amd64<br>- linux/ppc64le<br>- linux/s390x |

#### Ansible variables to be supplied to the playbook

| Name | Description |
| ---  | ---         |
| arch | The architecture the images are being built for, currently the supported values are:<br>- amd64<br>- ppc64le<br>- s390x |
| stackrox_io_username | Username used for pushing images to quay.io/stackrox-io |
| stackrox_io_password | Password used for pushing images to quay.io/stackrox-io |
| rhacs_eng_username | Username used for pushing images to quay.io/rhacs-eng |
| rhacs_eng_password | Password used for pushing images to quay.io/rhacs-eng |

### Collector image

The `ci-build-collector.yml` playbook is meant to be used by CI, it handles
retagging and pushing of Collector images to quay.io.

#### Environment variables used by the playbook

| Name | Description |
| ---  | ---         |
| COLLECTOR_BUILDER_TAG | The tag used for the builder image. |
| PLATFORM | The platform the images are being built for, currently the supported values are:<br>- linux/amd64<br>- linux/ppc64le<br>- linux/s390x |
| COLLECTOR_TAG | The tag to be used with the collector image |
| RHACS_ENG_IMAGE | Similar to the collector_image ansible variable, but for the quay.io/rhacs-eng registry |

#### Ansible variables to be supplied to the playbook

| Name | Description |
| ---  | ---         |
| collector_image | The collector image name to be built, including its tag |
| arch | The architecture the images are being built for, currently the supported values are:<br>- amd64<br>- ppc64le<br>- s390x |
| stackrox_io_username | Username used for pushing images to quay.io/stackrox-io |
| stackrox_io_password | Password used for pushing images to quay.io/stackrox-io |
| rhacs_eng_username | Username used for pushing images to quay.io/rhacs-eng |
| rhacs_eng_password | Password used for pushing images to quay.io/rhacs-eng |

## Integration tests
### Overview

The top-level yaml files define playbooks to perform various actions, and can
be run either directly with `ansible-playbook` or through the Makefile targets.

For example, to create and provision RHEL VMs for testing:

```bash
$ VM_TYPE=rhel ansible-playbook -i dev vm-lifecycle.yml --tags setup

# note you only need to specify VM_TYPE when creating VMs
$ ansible-playbook -i dev vm-lifecycle.yml --tags provision
```

Alternatively you can use the `integration-tests.yml` playbook to handle
creation and deletion of VMs as well as running the integration tests against
them:

```bash
$ VM_TYPE=rhel ansible-playbook -i dev integration-tests.yml
```

Note: the `integration-tests.yml` playbook also supports the `setup`, `provision`
`run-tests` and `teardown` tags to run each stage of the process.


### VMs

Within `vars/all.yml` there exists configuration for a variety of VM types,
summarized below:

| Type          | Families       |
| ------------- | -------------- |
| rhel          | rhel-7 <br> rhel-8 |
| rhel-s390x    | rhel-8-6-s390x |
| rhel-ppc64le  | rhel-8.8-05102023 |
| rhel-sap      | rhel-8-4-sap-ha <br> rhel-8-6-sap-ha |
| cos           | cos-stable <br> cos-beta <br> cos-dev |
| sles          | sles-12 <br> sles-15 |
| ubuntu-os     | ubuntu-1804-lts <br> ubuntu-2004-lts <br> ubuntu-2204-lts |
| ubuntu-os-pro | ubuntu-pro-1804-lts |
| fedora-coreos | fedora-coreos-stable |
| flatcar       | flatcar-stable |
| garden-linux  | n/a (built from a specific image, see `group_vars/all.yml` for details) |

By specifying `VM_TYPE=<type>` the playbooks will create a VM for every family
listed above. The name of the VMs is defined by the inventory-specific prefix,
the VM family, and the unique ID (which defaults to the current username).

```
<inventory prefix>-<family>-<id>

# e.g.

# A rhel-7 dev VM
collector-dev-rhel-7-12345

# A flatcar-stable CI VM
collector-ci-flatcar-stable-12345
```

### Inventory

The inventory is [dynamic](https://docs.ansible.com/ansible/latest/user_guide/intro_dynamic_inventory.html)
which means that the hosts and groups are all derived from GCP. The `ci/` and `dev/`
subdirectories are [inventory directories](https://docs.ansible.com/ansible/latest/user_guide/intro_dynamic_inventory.html#using-inventory-directories-and-multiple-inventory-sources)
and search the relevant GCP project for VMs that match the unique identifier. They are then grouped
based on that identifier as well as their platform.

The two groups are `job_id_<unique id>` and `platform_<vm type>`

These groups are used to run against only the VMs that have been created by the
same user or CI job.

### Environment Variables

The following environment variables may be used to modify some behavior:

| Variable | Description | Default |
| -------- | ----------- | ------- |
| GCP_SSH_KEY_FILE | The location of the private key file to use with GCP | ~/.ssh/google_compute_engine |
| IBMCLOUD_SSH_KEY_FILE | The location of the private key file to use with IBM Cloud | ~/.ssh/acs-sshkey_rsa.prv |
| JOB_ID | A unique identifier to de-conflict VM names | the current user's username |
| COLLECTOR_TEST | Which integration test make target to run. (e.g. integration-test-process-network) | ci-integration-tests |
| VM_TYPE | Which kind of VMs to create on GCP (as listed above.) By default, will only build rhel VMs. Use 'all' to build an inventory containing every kind of VM. | rhel |
| IC_API_KEY | API key for IBM Cloud. See [doc](https://cloud.ibm.com/docs/account?topic=account-userapikey&interface=ui) |
| IC_REGION | The IBM Cloud region where you want to create your resources. See [doc](https://cloud.ibm.com/docs/workload-protection?topic=workload-protection-regions)|

Note: other environment variables that may affect the operation of the integration tests
can be used to modify behavior. See [the integration tests README](../integration-tests/README.md) for details.

### Secrets & Creds

The make targets expect some secrets to exist within a secrets.yml file in the
root of this directory structure. It should contain key/value pairs of variable
names and credentials to be used in the playbooks. Currently, the only required
credentials are quay_username and quay_password, which are created by make
from the environment variables `QUAY_RHACS_ENG_RO_USERNAME` and `QUAY_RHACS_ENG_RO_PASSWORD`
to match CI variables. If you are using IBM Cloud to create IBM  RHEL instances, you will also need to specifiy enviroment variables `REDHAT_USERNAME` and `REDHAT_PASSWORD` to register your RHEL system.

To create your own, for dev the format should be:

```yaml
---
quay_username: "<quay username>"
quay_password: "<quay_password>"
```

You can also encrypt this file using ansible-vault, should you wish to password
protect it, though you will need to input the password every time it is used.

```bash
$ ansible-vault encrypt secrets.yml
```

This will require some changes to any `ansible-playbook` commands, as you
need to provide a password. Add `--ask-vault-pass` to any commands.

### Available Roles

#### create-vm

This role allows a playbook to create a VM on GCP or IBM Cloud (for IBM Z VMs). It is expected to run on
a host that has the GCP SDK or IBM Cloud ansible collection available (which is normally localhost).

See [create-all-vms](./roles/create-all-vms/tasks/main.yml) for an example of how it's used.

Note that, for IBM Z VMs, the VM creation process utilizes some pre-created resources on IBM Cloud, such as [VPC](https://cloud.ibm.com/docs/vpc), [Resource Group](https://cloud.ibm.com/docs/account?topic=account-rgs&interface=ui), ssh-key pair, subnet, images. Once a VM is created, a [Floating IP](https://cloud.ibm.com/docs/vpc?topic=vpc-fip-about) will be created and assigned to the VM for public access.

#### create-all-vms

Higher level role which creates all VMs from a given list. Calls into create
VM for the heavy lifting.

See [integration-tests.yml](./integration-tests.yml) or [benchmarks.yml](./benchmarks.yml) for
examples of how it's used.

#### provision-vm

This role will setup a VM with docker and get it ready for testing. It includes
platform-specific logic, as well as bootstrapping of python/ansible on Flatcar, which
does not have python installed by default.

See [vm-lifecycle.yml](./vm-lifecycle.yml) for an example of how it's used.

#### destroy-vm

This role will delete a VM from GCP or IBM Cloud, based on its instance name.

See [vm-lifecycle.yml](./vm-lifecycle.yml) for an example of how it's used.

Note that, for IBM VMs, the destroy process will only delete the VM and release its floating IP. Other shared resources mentioned in the creation process above will not be removed.

#### run-test-target

Handles running a make target in the [integration-tests](../integration-tests) directory
in the repository. Will attempt to run the target for eBPF collection
except in cases where a VM is designated for a specific collection method (via the vm_collection_method label)

See [integration-tests.yml](./integration-tests.yml) or [benchmarks.yml](./benchmarks.yml) for
examples of how it's used.

### Layout

The structure of subdirectories and files largely conform to a common [ansible
directory format](https://docs.ansible.com/ansible/2.8/user_guide/playbooks_best_practices.html#content-organization).

| Directory  | Purpose                                            |
| ---------  | -------------------------------------------------- |
| roles      | Common sets of tasks that can be used by playbooks |
| group_vars | Variables for specific groups (or all groups), the filename is the group name. e.g. platform_flatcar.yml affects the platform_flatcar group |
| ci, dev    | These are [inventory directories](https://docs.ansible.com/ansible/latest/user_guide/intro_dynamic_inventory.html#using-inventory-directories-and-multiple-inventory-sources) that contain different variables based on where the playbooks are running. |
| tasks      | Yaml files that contain task lists |
| .          | The root of this directory is where playbooks should exist |

To find VM definitions see [group_vars/all.yml](./group_vars/all.yml)

[^1]: there should be no CI related functionality within this directory
      outside the `ci/` inventory.

### K8S based integration tests
There are a set of tests that can be run on k8s clusters. In order to make
these as easy as possible to execute, there is an ansible playbook at
`$REPO_ROOT/ansible/k8s-integration-tests.yml`. This playbook runs solely on
ansible variables and attempts to be as flexible and as easy to use as
possible, so they also allow the tests to run on a throw away KinD cluster that
is created and deleted by the playbook, or on an existing cluster of your own.

The following section will describe how to configure and run these
tests.

#### Inventory file
As with every ansible playbook, an inventory file is required. The k8s tests
are prepared to use the local kubeconfig, so if you want to run the tests on an
existing cluster or by using a throw away one with KinD, the following
inventory should be enough:

```yaml
all:
  hosts:
    local:
      ansible_connection: local
      ansible_python_interpreter: "{{ansible_playbook_python}}"
```

For other configurations that require access to remote systems, refer to
[ansible's guide on managing inventories](https://docs.ansible.com/ansible/latest/inventory_guide/intro_inventory.html).

#### Variables
The following is a list of variables used by the playbook.

| Variable Name | Description |
| --- | --- |
| tester_image | The image to run the tests, created from the `build-image` integration-tests make target. |
| collector_image | The collector image to be tested. |
| collector_root | The path to the root of the collector repo, used for dumping logs. |
| cluster_name | When using a KinD cluster, the name to be used for the cluster where the tests will run. Default: collector-tests |
| container_engine | Specify the container engine to be used checking for local images to upload to KinD. Default: docker |

Easiest way to manage these variables is to create a yaml file that can later
be supplied to the `ansible-playbook` command, the following should work as a
template for such a file

```yaml
---
tester_image: <tester image>
collector_image: <collector image>
collector_root: <path to the collector repo>
cluster_name: collector-tests
```

#### Tags
The playbook also has a some tags that allow for more flexibility in execution,
keep in mind that if the `--tags` argument is not supplied to
`ansible-playbook` it will run all steps in the playbook.

| Tag name | Description |
| --- | --- |
| test-only | Run the tests and clean up the environment, skip all steps handling disposable KinD clusters. |
| cleanup | Run the cleanup steps only, removing k8s objects such as namespaces, cluster roles, etc... |

#### Examples
##### Run all tests on a throw-away KinD cluster

Assuming your inventory is in `inventory.yml` and variables are set in a
`k8s-tests.yml` file, the following command will create a KinD cluster, run
all integration tests and teardown the cluster.

```sh
ansible-playbook \
    -i inventory.yml \
    -e '@k8s-tests.yml' \
    k8s-integration-tests.yml
```

##### Run the tests on an existing cluster

The following command will only run the integration tests, once the playbook
is done executing the cluster will be left in the same state it was before
running it.

```sh
ansible-playbook \
    -i inventory.yml \
    -e '@k8s-tests.yml' \
    --tags test-only \
    k8s-integration-tests.yml
```

##### Run the cleanup steps on a cluster

In case something goes wrong and you want to remove all objects created by the
playbook from your cluster, the `cleanup` tag can be used.

```sh
ansible-playbook \
    -i inventory.yml \
    -e '@k8s-tests.yml' \
    --tags cleanup \
    k8s-integration-tests.yml
```

##### Running the playbook step-by-step

This is more of a general ansible-playbook tip, but still useful for debugging.
Using the `--step` argument will cause the ansible-playbook command to stop at
every step and wait for user confirmation.

```sh
ansible-playbook \
    -i inventory.yml \
    -e '@k8s-tests.yml' \
    --step \
    k8s-integration-tests.yml
```
For more tips on troubleshooting ansible playbooks see
[Executing playbooks for troubleshooting](https://docs.ansible.com/ansible/latest/playbook_guide/playbooks_startnstep.html).
