**Warning this specification and its implementation are alpha**
**This means that the spec, implementation and apis are subject to change
without any notice, the API may change in incompatible ways and support for
the feature may be dropped at any time without notice**

# Mount Plugins Specification

In the context of enabling nvidia-docker on Jetson, this doc specifies the
format and behavior of mount plugins. These are files that direct
libnvidia-container to specific paths to be mounted from the host into the
container.

## Background

The NVIDIA Software stack, so that it can ultimately run GPU code, talks to
the NVIDIA driver through a number of userland libraries (e.g: libcuda.so).
Because the driver API is not stable, these libraries are shipped and installed
by the NVIDIA driver (i.e: every NVIDIA driver has it's own libcuda).

In effect, what that means is that having a container which contains these
libraries, ties it to the driver version it was built and ran against.
Therefore moving that container to another machine becomes impossible.
The approach we decided to take is to mount, at runtime, these libraries from
your host filesystem into your container.

Additionally, the Jetson platform possesses it's own set of challenges,
e.g: Image size, libraries being tied to specific hardware units (e.g: nvDLA)
and platform specific libs (Nano vs Xavier).
Traditionally on x86, the set of libraries that would need to be mounted from
the host is fairly limited and could be hard coded.
On Jetson, that model just doesn't scale and requires more flexibility.

These challenges are the basis for the mount plugin system described here.

## Mount Plugin Loading

An nvidia-docker mount plugin is a ".csv" terminated file which sits in the
host filesystem at /etc/nvidia-container-runtime/host-files-for-container.d

This file describes all library, directories, devices and symlinks that need
to be mounted inside the container.
*Note: The mount plugin system will not be recursively iterating over the
directory*

When libnvidia-container is called during container creation, it will list
the files in the above directory and extract all the paths specified by these
files.
It will then proceed to mount the files inside the container as follows:
 - **Regular files**: will be mounted in read-only mode at the same path as
                      the one on the host.
                      If a component of the path doesn't exist, it will be
                      created in rw mode
 - **Directories**: will be mounted in read-only mode
 - **Devices**: will be mounted with the same mode as the host
 - **Symlinks**: The end target will be mounted inside the container.

*Note: If the path is non existent it will be ignored (but logged)*

## Mount Plugin File Format Specification V1 (R32.2)
The mount plugin filename MUST end with a ".csv" extension.
The mount plugin filename MUST be located at /etc/nvidia-container-runtime/host-files-for-container.d/

The mount plugin file is a Comma Separated Values file where the first key
is the type of file that must be mounted and the second key is the path.

Four keys are supported: dev, lib, sym and dir
e.g:
```
dev, /dev/nvhost-vic
lib, /usr/lib/aarch64-linux-gnu/libnvinfer.so.5.1.6
sym, /usr/lib/aarch64-linux-gnu/libnvinfer.so.5
dir, /usr/src/tensorrt
```
