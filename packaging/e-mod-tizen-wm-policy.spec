%bcond_with x
Name:       e-mod-tizen-wm-policy 
Summary:    The Enlightenment WM Policy Module for Tizen
Version:    0.1.0
Release:    1
Group:      System/GUI/Other
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
%make_install

%files
%defattr(-,root,root,-)
%{_libdir}/enlightenment/modules/e-mod-tizen-wm-policy

%define _unpackaged_files_terminate_build 0
