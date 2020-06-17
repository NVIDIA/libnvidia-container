Name: libnvidia-container
License:        BSD-3-Clause AND Apache-2.0 AND GPL-3.0-or-later AND LGPL-3.0-or-later AND MIT AND GPL-2.0-only
# elftoolchain is licensed under BSD-3-Clause
#  https://github.com/elftoolchain/elftoolchain#copyright-and-license
# libnvidia-container is licensed under apache-2.0
#  https://github.com/NVIDIA/libnvidia-container/blob/master/LICENSE
# libnvidia-container includes the GLPv3 license
#  https://github.com/NVIDIA/libnvidia-container/blob/master/COPYING
# libnvidia-container includes the LGPLv3 license
#  https://github.com/NVIDIA/libnvidia-container/blob/master/COPYING.LESSER
# nvidia-modprobe is licensed under GPLv2
#  https://github.com/NVIDIA/nvidia-modprobe/blob/master/COPYING
# several nvidia-modprobe files contain the MIT license header
#  https://github.com/NVIDIA/nvidia-modprobe/blob/master/utils.mk
Vendor: NVIDIA CORPORATION
Packager: NVIDIA CORPORATION <cudatools@nvidia.com>
URL: https://github.com/NVIDIA/libnvidia-container
BuildRequires: make
Version: %{_version}
%{!?_tag: %define _release 1}
%{?_tag:  %define _release 0.1.%{_tag}}
Release: %{_release}
Summary: NVIDIA container runtime library
%description
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

%install
DESTDIR=%{buildroot} %{__make} install prefix=%{_prefix} exec_prefix=%{_exec_prefix} bindir=%{_bindir} libdir=%{_libdir} includedir=%{_includedir} docdir=%{_licensedir}

%package -n %{name}%{_major}
Summary: NVIDIA container runtime library
%description -n %{name}%{_major}
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package requires the NVIDIA driver (>= 340.29) to be installed separately.
%post -n %{name}%{_major} -p /sbin/ldconfig
%postun -n %{name}%{_major} -p /sbin/ldconfig
%files -n %{name}%{_major}
%license %{_licensedir}/*
%{_libdir}/lib*.so.*

%package devel
Requires: %{name}%{_major}%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (development files)
%description devel
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains the files required to compile programs with the library.
%files devel
%license %{_licensedir}/*
%{_includedir}/*.h
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/*.pc

%package static
Requires: %{name}-devel%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (static library)
%description static
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package requires the NVIDIA driver (>= 340.29) to be installed separately.
%files static
%license %{_licensedir}/*
%{_libdir}/lib*.a

%define debug_package %{nil}
%package -n %{name}%{_major}-debuginfo
Requires: %{name}%{_major}%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (debugging symbols)
%description -n %{name}%{_major}-debuginfo
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains the debugging symbols for the library.
%files -n %{name}%{_major}-debuginfo
%license %{_licensedir}/*
%{_prefix}/lib/debug%{_libdir}/lib*.so.*

%package tools
Requires: %{name}%{_major}%{?_isa} >= %{version}-%{release}
Summary: NVIDIA container runtime library (command-line tools)
%description tools
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains command-line tools that facilitate using the library.
%files tools
%license %{_licensedir}/*
%{_bindir}/*

%changelog
* Thu Jun 11 2020 NVIDIA CORPORATION <cudatools@nvidia.com> 1.2.0-0.1.rc.1
- 4263e684 Add support for Windows Subsystem for Linux (WSL2)
- 2b75a55e Fix ability to build RC packages via TAG=rc.<num>

* Tue May 19 2020 NVIDIA CORPORATION <cudatools@nvidia.com> 1.1.1-1
- deeb499 Fixup deb packaging files to remove warnings
- 6003504 nvmlSystemGetCudaDriverVersion_v2 to nvmlSystemGetCudaDriverVersion
- 1ee8b60 Update centos8/rhel8 to conditionally set appropriate CFLAGS and LDLIBS
- d746370 Add smoke test to verify functioning build for all OSs on amd64/x86_64

* Fri May 15 2020 NVIDIA CORPORATION <cudatools@nvidia.com> 1.1.0-1
- b217c6ad Update build system to support multi-arch builds
- 1ddcdfc1 Add support for MIG (Milti-Instance-GPUs)
- ddae363a Add libnvidia-allocator.so as a compute-lib
- 6ed0f129 Add option to not use pivot_root
- e18e9b7a Allow devices to be identified by PCI bus ID

* Mon Nov 11 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.7-1
- 8d90918a Add Raytracing library

* Fri Sep 013 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.6-1
- b6aff41 Update error messages for CUDA version requirements

* Wed Sep 04 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.5-1
- 688495e Add Opensuse15.1 support

* Wed Aug 21 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.4-1
- 61bfaf38 Update DSL to output the first element instead of the last in case of failure
- 5ce32c6c Add initial support for Optix
- acc38a22 Fix execveat typo
- b5e491b1 arm64: Add support for AARCH64 architecture

* Thu Jul 18 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.3-1
- b9545d7 Add support for Vulkan

* Tue Feb 05 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.2-1
- 4045013 Adds support for libnvidia-opticalflow

* Mon Jan 14 2019 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.1-1
- deccb28 Allow yet more syscalls in ldconfig

* Thu Sep 20 2018 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-1
- 35a9f27 Add support for CUDA forward compatibility
- ebed710 Add device brand to the device informations and requirements
- a141a7a Handle 32-bit PCI domains in procfs
- 391c4b6 Preload glibc libraries before switching root
- bcf69c6 Bump libtirpc to 1.1.4
- 30aec17 Bump nvidia-modprobe-utils to 396.51
- d05745f Bump the address space limits for ldconfig

* Mon Jun 11 2018 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.rc.2
- 7ea554a Rework capabilities to support more unprivileged use-cases
- f06cbbb Fix driver process DEATHSIG teardown
- 931bd4f Allow more syscalls in ldconfig
- a0644ea Fix off-by-one error

* Thu Apr 26 2018 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.rc.1
- 4d43665 Bump nvidia-modprobe-utils to 396.18
- d8338a6 Bump libtirpc to 1.0.3
- cef6c8f Add execveat to the list of allowed syscalls

* Mon Mar 05 2018 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.beta.1
- 6822b13 Bump nvidia-modprobe-utils to 390.25
- 8245f6c Slightly improve RPC error messages
- 9398d41 Add support for display capability
- 57a0dd5 Increase driver service timeout from 1s to 10s
- e48a0d4 Add device minor to the CLI info command
- 019fdc1 Add support for custom driver root directory
- b78a28c Add ppc64le support
- 41656bf Add --ldcache option to the CLI

* Wed Jan 10 2018 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.alpha.3
- d268f8f Improve error message if driver installed in the container
- 3fdac29 Add optional support for libelf from the elfutils project
- 584bca5 Remove top directory bind mounts to prevent EXDEV errors
- c6dc820 Add info command to nvidia-container-cli
- 44b74ee Add device model to the device informations
- cbdd58f Strip RPC prefix from error messages
- d4ee216 Rework the CLI list command
- b0c4865 Improve the --userspec CLI option and rename it to --user
- e6fa331 Refactor the CLI and split it into multiple files
- fa9853b Bump nvidia-modprobe-utils to 387.34
- 7888296 Move the driver capabilities to the container options
- ea2f780 Add support for EGL device isolation
- b5bffa3 Fix driver procfs remount to work with unpatched kernels 

* Mon Oct 30 2017 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.alpha.2
- b80e4b6 Relax some requirement constraints
- 3cd1bb6 Handle 32-bit PCI domains
- 6c67a19 Add support for device architecture requirement
- 7584e96 Filter NVRM proc filesystem based on visible devices
- 93c46e1 Prevent the driver process from triggering MPS
- fe4925e Reject invalid device identifier "GPU-"
- dabef1c Do not change bind mount attributes on top-level directories

* Tue Sep 05 2017 NVIDIA CORPORATION <cudatools@nvidia.com> 1.0.0-0.1.alpha.1
- Initial release
