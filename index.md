# Repository configuration

In order to setup the libnvidia-container repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/libnvidia-container/issues/new).

List of supported distributions:

|  OS Name / Version   |  Identifier | amd64 / x86_64 | ppc64le | arm64 / aarch64 |
| -------------------- | :---------: | :------------: | :-----: | :-------------: |
| Amazon Linux 2       | amzn2       |       ✔        |         |                 |
| Amazon Linux 2017.09 | amzn2017.09 |       ✔        |         |                 |
| Amazon Linux 2018.03 | amzn2018.03 |       ✔        |         |                 |
| Open Suse Leap 15.0  | sles15.0    |       ✔        |         |                 |
| Open Suse Leap 15.1  | sles15.1    |       ✔        |         |                 |
| Debian Linux 9       | debian9     |       ✔        |         |                 |
| Debian Linux 10      | debian10    |       ✔        |         |                 |
| Debian Linux 11      | debian11    |       ✔        |         |                 |
| Centos 7             | centos7     |       ✔        |    ✔    |                 |
| Centos 8             | centos8     |       ✔        |    ✔    |        ✔        |
| RHEL 7.4             | rhel7.4     |       ✔        |    ✔    |                 |
| RHEL 7.5             | rhel7.5     |       ✔        |    ✔    |                 |
| RHEL 7.6             | rhel7.6     |       ✔        |    ✔    |                 |
| RHEL 7.7             | rhel7.7     |       ✔        |    ✔    |                 |
| RHEL 7.8             | rhel7.8     |       ✔        |    ✔    |                 |
| RHEL 7.9             | rhel7.9     |       ✔        |    ✔    |                 |
| RHEL 8.0             | rhel8.0     |       ✔        |    ✔    |        ✔        |
| RHEL 8.1             | rhel8.1     |       ✔        |    ✔    |        ✔        |
| RHEL 8.2             | rhel8.2     |       ✔        |    ✔    |        ✔        |
| RHEL 8.3             | rhel8.3     |       ✔        |    ✔    |        ✔        |
| RHEL 8.4             | rhel8.4     |       ✔        |    ✔    |        ✔        |
| RHEL 8.5             | rhel8.5     |       ✔        |    ✔    |        ✔        |
| Ubuntu 16.04         | ubuntu16.04 |       ✔        |    ✔    |                 |
| Ubuntu 18.04         | ubuntu18.04 |       ✔        |    ✔    |        ✔        |
| Ubuntu 19.04         | ubuntu19.04 |       ✔        |    ✔    |        ✔        |
| Ubuntu 19.10         | ubuntu19.10 |       ✔        |    ✔    |        ✔        |
| Ubuntu 20.04         | ubuntu20.04 |       ✔        |    ✔    |        ✔        |
| Ubuntu 22.04         | ubuntu22.04 |       ✔        |    ✔    |        ✔        |

## Debian-based distributions

```bash
distribution=$(. /etc/os-release;echo $ID$VERSION_ID) \
         && curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg \
         && curl -s -L https://nvidia.github.io/libnvidia-container/$distribution/libnvidia-container.list | \
               sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
               sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
sudo apt-get update
```

For pre-releases, you need to enable the experimental repos:
```bash
sudo sed -i -e '/experimental/ s/^#//g' /etc/apt/sources.list.d/libnvidia-container.list
sudo apt-get update
```

To later disable the experimental repos, you can run:
```bash
sudo sed -i -e '/experimental/ s/^/#/g' /etc/apt/sources.list.d/libnvidia-container.list
sudo apt-get update
```

## RHEL-based distributions

```bash
DIST=$(. /etc/os-release; echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/libnvidia-container/$DIST/libnvidia-container.repo | \
  sudo tee /etc/yum.repos.d/libnvidia-container.repo
```

For pre-releases, you need to enable the experimental repos:
```bash
sudo yum-config-manager --enable libnvidia-container-experimental
```

To later disable the experimental repos, you can run:
```bash
sudo yum-config-manager --disable libnvidia-container-experimental
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
distribution=$(. /etc/os-release;echo $ID$VERSION_ID) \
         && curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg
```
