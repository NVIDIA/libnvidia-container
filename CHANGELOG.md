# NVIDIA Container Toolkit Library and CLI Changelog

* Fix device permission check when using cgroupv2 (fixes #227)

## 1.14.1
* Use libelf.so on RPM-based systems due to removed mageia repositories hosting pmake and bmake.

## 1.14.0
* Promote 1.14.0~rc.3 to 1.14.0
## 1.14.0~rc.3
* Generate debian and RPM changelogs automatically.

## 1.14.0~rc.2
* Include Shared Compiler Library (`libnvidia-gpucomp.so``) in the list of compute libaries.

## 1.14.0~rc.1
* Remove `linvidia-container0`` dependency on Ubuntu-based arm64 platforms
* Support OpenSSL 3 with the Encrypt/Decrypt library
