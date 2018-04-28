# Repository configuration

In order to setup the libnvidia-container repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/libnvidia-container/issues/new).

List of supported distributions:
- Ubuntu 14.04, 16.04, 18.04
- Debian 8, 9
- CentOS 7
- Red Hat Enterprise Linux 7
- Amazon Linux 1, 2

## Debian-based distributions

```bash
DIST=$(. /etc/os-release; echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo apt-key add -
curl -s -L https://nvidia.github.io/libnvidia-container/$DIST/libnvidia-container.list | \
  sudo tee /etc/apt/sources.list.d/libnvidia-container.list
sudo apt-get update
```

## RHEL-based distributions

```bash
DIST=$(. /etc/os-release; echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/libnvidia-container/$DIST/libnvidia-container.repo | \
  sudo tee /etc/yum.repos.d/libnvidia-container.repo
```

## Other distributions

```bash
# Where $RELEASE points to a tarball downloaded from
# https://github.com/NVIDIA/libnvidia-container/releases
sudo tar --strip-components=1 -C / -xvf $RELEASE
```
