# Repository configuration

In order to setup the libnvidia-container repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/libnvidia-container/issues/new).

List of supported distributions:

|         | Ubuntu 14.04 | Ubuntu 16.04 | Ubuntu 18.04 | Debian 8 | Debian 9 | Centos 7 | RHEL 7 | Amazon Linux 1 | Amazon Linux 2 |
| ------- | :----------: | :----------: | :----------: | :------: | :------: | :------: | :----: | :------------: | :------------: |
| x86_64  |      X       |      X       |       X      |     X    |    X     |    X     |    X   |        X       |        X       |
| ppc64le |              |      X       |       X      |          |    X     |    X     |    X   |                |                |

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

# Updating repository keys

In order to update the libnvidia-container repository key for your distribution, follow the instructions below.

## RHEL-based distributions

```bash
DIST=$(sed -n 's/releasever=//p' /etc/yum.conf)
DIST=${DIST:-$(. /etc/os-release; echo $VERSION_ID)}
sudo rpm -e gpg-pubkey-f796ecb0
sudo gpg --homedir /var/lib/yum/repos/$(uname -m)/$DIST/libnvidia-container/gpgdir --delete-key f796ecb0
sudo yum makecache
```

## Debian-based distributions
```bash
curl -s -L https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo apt-key add -
```
