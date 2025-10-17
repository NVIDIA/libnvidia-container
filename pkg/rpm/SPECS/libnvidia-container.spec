Name: libnvidia-container
License:        BSD-3-Clause AND Apache-2.0 AND GPL-3.0-or-later AND LGPL-3.0-or-later AND MIT AND GPL-2.0-only
# elftoolchain is licensed under BSD-3-Clause
#  https://github.com/elftoolchain/elftoolchain#copyright-and-license
# libnvidia-container is licensed under apache-2.0
#  https://github.com/NVIDIA/libnvidia-container/blob/main/LICENSE
# libnvidia-container includes the GPLv3 license
#  https://github.com/NVIDIA/libnvidia-container/blob/main/COPYING
# libnvidia-container includes the LGPLv3 license
#  https://github.com/NVIDIA/libnvidia-container/blob/main/COPYING.LESSER
# nvidia-modprobe is licensed under GPLv2
#  https://github.com/NVIDIA/nvidia-modprobe/blob/main/COPYING
# several nvidia-modprobe files contain the MIT license header
#  https://github.com/NVIDIA/nvidia-modprobe/blob/main/utils.mk
Vendor: NVIDIA CORPORATION
Packager: NVIDIA CORPORATION <cudatools@nvidia.com>
URL: https://github.com/NVIDIA/libnvidia-container
BuildRequires: make
Version: %{_version}
Release: %{_release}%{?dist}
Summary: NVIDIA container runtime library
%description
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

%prep

%build

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
Requires: %{name}%{_major}%{?_isa} = %{version}-%{release}
Summary: NVIDIA container runtime library (command-line tools)
%description tools
The nvidia-container library provides an interface to configure GNU/Linux
containers leveraging NVIDIA hardware. The implementation relies on several
kernel subsystems and is designed to be agnostic of the container runtime.

This package contains command-line tools that facilitate using the library.
%files tools
%license %{_licensedir}/*
%{_bindir}/*

%package libseccomp2
Requires: libseccomp2
Provides: libseccomp.so = %{version}-%{release}
Conflicts: libseccomp.so
Summary: A virtual package to provide libseccomp through libseccomp2
%description libseccomp2
This is a virtual package to satisfy the libseccomp.so dependency through a
transitive dependency on libseccomp2.so.
%files libseccomp2
%license %{_licensedir}/*

%changelog
# As of 1.14.0-1 we generate the release information automatically
* %{release_date} NVIDIA CORPORATION <cudatools@nvidia.com> %{version}-%{release}
- See https://gitlab.com/nvidia/container-toolkit/libnvidia-container/-/blob/%{git_commit}/CHANGELOG.md
