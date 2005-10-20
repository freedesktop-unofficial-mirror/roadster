# Define ndiswrapper_version only if it is not already defined.
%{!?roadster_version: %define roadster_version 0.3.7PR}
%{!?roadster_release: %define roadster_release 1}

Summary: roadster allows you to view and navigate the US without the internet.
Name: roadster
Version: %{roadster_version}
Release: %{roadster_release}
License: GPL
Group: System Environment/Base
URL: http://linuxadvocate.org/projects/roadster/
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: mysql-devel gtk2 automake autoconf
Requires: gtk2

%description
This package allows you to navigate the US and view a roadmap
for any arbitrary place.

%prep
%setup -q
./autogen.sh --prefix='/usr'

%build
make

%install

%define inst_dir $RPM_BUILD_ROOT%{_inst_dir}
%define sbindir $RPM_BUILD_ROOT%{_sbinrootdir}
%define usrsbindir $RPM_BUILD_ROOT%{_sbindir}
%define mandir $RPM_BUILD_ROOT%{_mandir}

rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT INST_DIR=%{inst_dir} sbindir=%{sbindir} usrsbindir=%{usrsbindir} mandir=%{mandir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0755,root,root)
%{_bindir}/roadster
%{_datadir}/pixmaps/roadster-icon.png
%{_datadir}/roadster/layers.xml
%{_datadir}/roadster/roadster.glade
%{_datadir}/applications/roadster.desktop
%doc README

%changelog
* Tue Oct 11 2005 Ofer Achler <ofer.achler+ROADSTER@gmail.com> - 
- First release of roadster spec file
