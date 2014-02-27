Name:           weston-ivi-shell
Version:        0.1.1
Release:        0
Summary:        Weston IVI Shell
License:        MIT
Group:          Graphics & UI Framework/Wayland Window System
Url:            https://github.com/ntanibata/weston-ivi-shell/
Source0:         %name-%version.tar.xz
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
Requires:       weston >= 1.4

%description
A reference Weston shell designed for use in IVI systems.

%package devel
Summary: Development files for package %{name}
Group:   Graphics & UI Framework/Development
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

%prep
%setup -q
cp %{SOURCE1001} .

%build
# We only care about the ivi-shell related bits so disable anything
# unrelated.
%autogen --disable-static  --disable-libunwind --disable-xwayland --disable-xwayland-test --disable-x11-compositor --disable-rpi-compositor --disable-weston-launch --disable-clients --disable-wcap-tools
make %{?_smp_mflags}

%install
%make_install

# install example clients
install -m 755 clients/weston-simple-shm-ivi %{buildroot}%{_bindir}
install -m 755 clients/weston-simple-egl-ivi %{buildroot}%{_bindir}
install -m 755 clients/weston-flower-ivi %{buildroot}%{_bindir}
install -m 755 clients/weston-smoke-ivi %{buildroot}%{_bindir}
install -m 755 clients/weston-clickdot-ivi %{buildroot}%{_bindir}

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
%_bindir/weston-simple-shm-ivi
%_bindir/weston-simple-egl-ivi
%_bindir/weston-flower-ivi
%_bindir/weston-smoke-ivi
%_bindir/weston-clickdot-ivi

%changelog
