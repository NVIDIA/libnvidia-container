# Repository configuration

In order to setup the libnvidia-container repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/libnvidia-container/issues/new).

## Ubuntu distributions (Xenial x86_64)

```bash
curl -L https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo apt-key add -
curl -s -L https://nvidia.github.io/libnvidia-container/ubuntu16.04/amd64/libnvidia-container.list | \
  sudo tee /etc/apt/sources.list.d/libnvidia-container.list
sudo apt-get update
```

## Debian distributions (Stretch x86_64)

```bash
curl -L https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo apt-key add -
curl -s -L https://nvidia.github.io/libnvidia-container/debian9/amd64/libnvidia-container.list | \
  sudo tee /etc/apt/sources.list.d/libnvidia-container.list
sudo apt-get update
```

## CentOS distributions (RHEL7 x86_64)

```bash
curl -s -L https://nvidia.github.io/libnvidia-container/centos7/x86_64/libnvidia-container.repo | \
  sudo tee /etc/yum.repos.d/libnvidia-container.repo
```

## Other distributions (x86_64)

```bash
# Where $RELEASE points to a tarball downloaded from
# https://github.com/NVIDIA/libnvidia-container/releases
sudo tar --strip-components=1 -C / -xvf $RELEASE
```
