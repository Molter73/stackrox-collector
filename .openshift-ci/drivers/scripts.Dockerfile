FROM registry.fedoraproject.org/fedora:36

# This image copies all scripts and files required for the drivers build into
# '/scripts/'. This allows other builds to simply:
# COPY --from=scripts /scripts/ /scripts/

# Files required for task files creation
COPY /kernel-modules/dockerized/scripts/ /scripts/
COPY /kernel-modules/build/apply-blocklist.py /scripts/
COPY /kernel-modules/BLOCKLIST /scripts/
COPY /.openshift-ci/drivers/BLOCKLIST /scripts/dockerized/
COPY /kernel-modules/KERNEL_VERSIONS /KERNEL_VERSIONS

# Files required to build
COPY /kernel-modules/build/prepare-src /scripts/prepare-src.sh
COPY /kernel-modules/build/build-kos /scripts/
COPY /kernel-modules/build/build-wrapper.sh /scripts/compile.sh

# OSCI specific scripts
COPY /.openshift-ci/scripts /.openshift-ci/drivers/scripts/ /scripts/
