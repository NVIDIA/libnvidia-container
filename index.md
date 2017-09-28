# Repository configuration

In order to setup the libnvidia-container repository for your distribution, follow the instructions below.

If you feel something is missing or requires additional information, please let us know by [filing a new issue](https://github.com/NVIDIA/libnvidia-container/issues/new).

## Ubuntu distributions (Xenial x86_64)

```bash
curl -L https://nvidia.github.io/libnvidia-container/gpgkey | \
sudo apt-key add -
sudo tee /etc/apt/sources.list.d/libnvidia-container.list <<< \
"deb https://nvidia.github.io/libnvidia-container/ubuntu16.04/amd64 /"
sudo apt-get update
```

## CentOS distributions (RHEL7 x86_64)

```bash
sudo tee /etc/yum.repos.d/libnvidia-container.repo <<EOF
[libnvidia-container]
name=libnvidia-container
baseurl=https://nvidia.github.io/libnvidia-container/centos7/x86_64
repo_gpgcheck=1
gpgcheck=0
enabled=1
gpgkey=https://nvidia.github.io/libnvidia-container/gpgkey
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
EOF
```

## Other distributions (x86_64)

```bash
# Where $RELEASE points to a tarball downloaded from
# https://github.com/NVIDIA/libnvidia-container/releases
sudo tar --strip-components=1 -C / -xvf $RELEASE
```
