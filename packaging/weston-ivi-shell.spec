Name:           weston-ivi-shell
Version:        0.1.4
Release:        0
Summary:        Weston IVI Shell
License:        MIT
Group:          Graphics & UI Framework/Wayland Window System
Url:            https://github.com/ntanibata/weston-ivi-shell/
Source0:        %name-%version.tar.xz
Source1:        weston.ini
Source1001: 	weston-ivi-shell.manifest
BuildRequires:	autoconf >= 2.64, automake >= 1.11
BuildRequires:  libtool >= 2.2
BuildRequires:  libjpeg-devel
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(weston) >= 1.4
BuildRequires:  pkgconfig(cairo-egl) >= 1.11.3
BuildRequires:  pkgconfig(egl) >= 7.10
BuildRequires:  pkgconfig(mtdev) >= 1.1.0
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-egl)
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(xkbcommon) >= 0.3.0
BuildRequires:  pkgconfig(pangocairo) >= 1.34.0
Requires:       weston >= 1.4

%description
A reference Weston shell designed for use in IVI systems.

%package devel
Summary: Development files for package %{name}
Group:   Graphics & UI Framework/Development
Requires:  %{name} = %{version}
%description devel
This package provides header files and other developer releated files
for package %{name}.

%package clients
Summary: Sample clients for package %{name}
Group:   Graphics & UI Framework/Development
%description clients
This package provides a set of example wayland clients useful for
validating the functionality of wayland with very little dependencies
on other system components.

%package config
Summary:    Tizen IVI Weston configuration for package %{name}
Group:      Automotive/Configuration
Requires:   weston-clients
Requires:   weekeyboard
Requires:   genivi-shell
Conflicts:  weston-ivi-config
Conflicts:  ico-uxf-weston-plugin
%description config
This package contains Tizen IVI-specific configuration for %{name}.

%prep
%setup -q
cp %{SOURCE1001} .

%build
# We only care about the ivi-shell related bits so disable anything
# unrelated.
%autogen --disable-static  --disable-libunwind --disable-xwayland --disable-xwayland-test --disable-x11-compositor --disable-rpi-compositor --disable-weston-launch --enable-clients --disable-wcap-tools
make %{?_smp_mflags}

%install
%make_install

# install example clients
mkdir -p %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-simple-shm-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-simple-egl-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-flower-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-smoke-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-clickdot-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-editor-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-dnd-ivi %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-simple-shm %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-simple-egl %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-flower %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-smoke %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-editor %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-dnd %{buildroot}%{_bindir}/ivi/
install -m 755 clients/weston-terminal %{buildroot}%{_bindir}/ivi/

%define weston_config_dir %{_sysconfdir}/xdg/weston
mkdir -p %{buildroot}%{weston_config_dir}
install -m 0644 %{SOURCE1} %{buildroot}%{weston_config_dir}

%define _unpackaged_files_terminate_build 0

%files
%manifest %{name}.manifest
%defattr(-,root,root)
%license COPYING
%_libdir/weston
%_datadir/weston

%files devel
%manifest %{name}.manifest
%_includedir/weston/*.h

%files clients
%manifest %{name}.manifest
%_bindir/ivi/weston-simple-shm-ivi
%_bindir/ivi/weston-simple-egl-ivi
%_bindir/ivi/weston-flower-ivi
%_bindir/ivi/weston-smoke-ivi
%_bindir/ivi/weston-clickdot-ivi
%_bindir/ivi/weston-dnd-ivi
%_bindir/ivi/weston-editor-ivi
%_bindir/ivi/weston-simple-shm
%_bindir/ivi/weston-simple-egl
%_bindir/ivi/weston-flower
%_bindir/ivi/weston-smoke
%_bindir/ivi/weston-editor
%_bindir/ivi/weston-dnd
%_bindir/ivi/weston-terminal

%files config
%manifest %{name}.manifest
%config %{weston_config_dir}/weston.ini

%changelog
