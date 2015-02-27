%bcond_with x
Name:       e-mod-tizen-wm-policy 
Summary:    The Enlightenment WM Policy Module for Tizen
Version:    0.1.8
Release:    1
Group:      Graphics & UI Framework/Other
License:    BSD-2-Clause
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(x11) 
BuildRequires: pkgconfig(enlightenment)
%if !%{with x}
ExclusiveArch:
%endif

%description
The Enlightenment WM Policy Module for Tizen

%prep
%setup -q -n %{name}-%{version}

%build
%autogen
%configure
make %{?_smp_mflags}

%install
# for license notification
mkdir -p %{buildroot}/usr/share/license
cp -a %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/usr/share/license/%{name}
cp -a %{_builddir}/%{buildsubdir}/COPYING.Flora %{buildroot}/usr/share/license/%{name}.Flora

# install
%make_install

%files
%defattr(-,root,root,-)
%{_libdir}/enlightenment/modules/e-mod-tizen-wm-policy
/usr/share/license/%{name}
/usr/share/license/%{name}.Flora

%define _unpackaged_files_terminate_build 0
