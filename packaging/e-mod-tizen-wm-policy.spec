%bcond_with x
%bcond_with wayland

Name:       e-mod-tizen-wm-policy
Summary:    The Enlightenment WM Policy Module for Tizen
Version:    0.2.29
Release:    1
Group:      Graphics & UI Framework/Other
License:    BSD-2-Clause and Flora-1.1
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(enlightenment)
BuildRequires: pkgconfig(ttrace)
BuildRequires: libsensord-devel
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(capi-system-device)
%if %{with x}
BuildRequires: pkgconfig(x11)
%endif
%if %{with wayland}
BuildRequires: pkgconfig(tizen-extension-server)
BuildRequires: pkgconfig(eina)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(edje)
BuildRequires: pkgconfig(tzsh-server)
BuildRequires: pkgconfig(cynara-client)
BuildRequires: pkgconfig(cynara-creds-socket)
%endif

%global TZ_SYS_RO_SHARE  %{?TZ_SYS_RO_SHARE:%TZ_SYS_RO_SHARE}%{!?TZ_SYS_RO_SHARE:/usr/share}

%description
The Enlightenment WM Policy Module for Tizen

%prep
%setup -q -n %{name}-%{version}

%build
export CFLAGS+=" -DE_LOGGING=1 -Werror-implicit-function-declaration"
%if %{with wayland}
%reconfigure \
      --enable-wayland-only \
      --enable-cynara \
      --enable-auto-rotation
%else
%reconfigure
%endif
make %{?_smp_mflags}

%install
# for license notification
mkdir -p %{buildroot}/%{TZ_SYS_RO_SHARE}/license
cp -a %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/%{TZ_SYS_RO_SHARE}/license/%{name}
cp -a %{_builddir}/%{buildsubdir}/COPYING.Flora %{buildroot}/%{TZ_SYS_RO_SHARE}/license/%{name}.Flora

# install
%make_install

%files
%defattr(-,root,root,-)
%{_libdir}/enlightenment/modules/e-mod-tizen-wm-policy
%{TZ_SYS_RO_SHARE}/license/%{name}
%{TZ_SYS_RO_SHARE}/license/%{name}.Flora

%define _unpackaged_files_terminate_build 0
