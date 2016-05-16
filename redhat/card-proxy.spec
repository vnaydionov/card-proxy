Name: card-proxy
Version: 0.5.7
Release: 2%{?dist}
Summary: Tokenizator for sensitive data with proper cryptography
Group: System Environment/Daemons
License: MIT
URL: https://github.com/vnaydionov/card-proxy
#Source: https://github.com/vnaydionov/%{name}/archive/%{version}.tar.gz
Source: %{version}.tar.gz

# Build patches
Patch1: card-proxy-rh-func.patch

BuildRequires: cmake python MySQL-python
BuildRequires: json-c-devel openssl-devel libcurl-devel
BuildRequires: boost-devel yborm-devel soci-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
This is a set of service API to provide secure vault for sensitive data
with properly implemented Tokenization, Encryption and Key Management.
Major focus is also on fault tolerance.


%package service
Summary: Background tasks plus shell functions for pinger/restarter/init scripts
Group: System Environment/Daemons
Requires: python MySQL-python
BuildArch: noarch

%description service
Set of Python scripts to be run in background mode for misc. service tasks
of the secure card vault.
Also some common Shell scripts used by other packages of the card-proxy.


%package tokenizer
Summary: Tokenizing proxy for card data, PCI-DSS compliant
Group: System Environment/Daemons
Requires: %{name}-service = %{version}-%{release} nginx18 soci-mysql

%description tokenizer
This is a HTTP proxy-server that stores the payment card data
sent by a user from a WEB-form or a mobile application in a secure card vault.
On receival of the card data the proxy passes to the upstream only an opaque
card token parameter.  And vice versa: when addressing the payment gate
the card token is automatically unwrapped to a full record.


%package keykeeper2
Summary: Distributed in-memory storage for one component of a master key.
Group: System Environment/Daemons
Requires: %{name}-service = %{version}-%{release} nginx18

%description keykeeper2
This is a simple HTTP server that keeps a portion of a key in memory,
automatically sharing the information between the specified peers.


%package confpatch
Summary: Helper for updating one component of master key that is stored in a file.
Group: System Environment/Daemons
Requires: %{name}-service = %{version}-%{release} nginx18

%description confpatch
This is a simple HTTP server that helps to maintain XML configs,
allowing to change the portions of confuguration files used by servants.


%package keyapi
Summary: Key management API for the secure card vault.
Group: System Environment/Daemons
Requires: %{name}-service = %{version}-%{release} nginx18 soci-mysql

%description keyapi
The key management server implements a HTTP API.
Most methods are authenticated by passing an existing part of KEK of some version.


%package secvault
Summary: A general purpose secure storage for sensitive data with tokenization.
Group: System Environment/Daemons
Requires: %{name}-service = %{version}-%{release} nginx18 soci-mysql

%description secvault
This is an HTTP server implementing API for tokenization of secrets.
Secrets are tokenized and stored encrypted in database, each secret can be
retrieved later using its token.  Secrets also have expiration timestamp
to notify about update needed.


%package tests
Summary: Set of test scripts to test different APIs of card proxy.
Group: System Environment/Daemons
Requires: %{name}-service = %{version}-%{release}
BuildArch: noarch

%description tests
Set of test scripts to test different APIs of card proxy.


%package keyman
Summary: An utility to control the process of master key switch.
Group: System Environment/Daemons
Requires: python
BuildArch: noarch

%description keyman
An utility to control the process of master key switch.
Typically this package is installed in DMZ segment, unlike the rest
of card proxy packages which reside in CDE network.


%prep
%setup -q -n %{version}
#cp %{SOURCE1} .

%patch1 -p1


%build

export SOURCE_DIR=`pwd`
export BUILD_ROOT=$SOURCE_DIR/build
mkdir -p $BUILD_ROOT
cd $BUILD_ROOT

cmake -D CMAKE_INSTALL_PREFIX=/usr -D YBORM_ROOT=/usr $SOURCE_DIR
make -j2


%install

export BUILD_ROOT=./build
cd $BUILD_ROOT

rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install

mv $RPM_BUILD_ROOT/usr/etc $RPM_BUILD_ROOT/

mkdir $RPM_BUILD_ROOT/usr/share/card_proxy_service/sql
mv $RPM_BUILD_ROOT/usr/share/card_proxy_tokenizer/*.sql \
    $RPM_BUILD_ROOT/usr/share/card_proxy_service/sql/
mv $RPM_BUILD_ROOT/usr/share/card_proxy_tokenizer/*.xml \
    $RPM_BUILD_ROOT/usr/share/card_proxy_service/sql/

mkdir $RPM_BUILD_ROOT/usr/share/doc/card-proxy-service
mv $RPM_BUILD_ROOT/usr/share/doc/card-proxy-tokenizer/* \
    $RPM_BUILD_ROOT/usr/share/doc/card-proxy-service

mkdir $RPM_BUILD_ROOT/usr/share/card_proxy_tests
mv $RPM_BUILD_ROOT/usr/share/card_proxy_tokenizer/tests/* \
    $RPM_BUILD_ROOT/usr/share/card_proxy_tests

%clean

rm -rf $RPM_BUILD_ROOT

export BUILD_ROOT=./build
rm -rf $BUILD_ROOT


%post service
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 1 ]; then
    ensure_user_exists 'cpr_service' 'CardProxy.Service'
    process_sample_file "${PROXY_COMMON}/key_settings.cfg.xml"
fi


%files service
%defattr(-,root,root)
%config %{_sysconfdir}/card_proxy_common/key_settings.cfg.xml.sample
%config(noreplace) %{_sysconfdir}/card_proxy_service/card_proxy_cleaner.cfg.xml
%config(noreplace) %{_sysconfdir}/card_proxy_service/card_proxy_hmacproc.cfg.xml
%config(noreplace) %{_sysconfdir}/card_proxy_service/card_proxy_keyproc.cfg.xml
%config(noreplace) %{_sysconfdir}/cron.d/card-proxy-service
%{_bindir}/card_proxy_cleaner.sh
%{_bindir}/card_proxy_hmacproc.sh
%{_bindir}/card_proxy_keyproc.sh
%{_libdir}/python2.6/site-packages/card_proxy_service/*.py*
/usr/share/card_proxy_service/*.sh
/usr/share/card_proxy_service/sql/*
%doc /usr/share/doc/card-proxy-service/README.md
%doc /usr/share/doc/card-proxy-service/LICENSE


%post tokenizer
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 1 ]; then
    ensure_user_exists 'cpr_tokenizer' 'CardProxy.Tokenizer'
    enable_nginx_config card_proxy_tokenizer
    register_autostart card_proxy_tokenizer
fi
#perform_service_restart card_proxy_tokenizer


%preun tokenizer
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 0 ]; then
    disable_nginx_config card_proxy_tokenizer
    service card_proxy_tokenizer stop
    unregister_autostart card_proxy_tokenizer
fi


%files tokenizer
%defattr(-,root,root)
%{_bindir}/card_proxy_tokenizer*
%{_sysconfdir}/init.d/card_proxy_tokenizer
%dir %{_sysconfdir}/card_proxy_tokenizer
%config(noreplace) %{_sysconfdir}/card_proxy_tokenizer/card_proxy_tokenizer.cfg.xml
%config(noreplace) %{_sysconfdir}/nginx/sites-available/10-card_proxy_tokenizer


%post keykeeper2
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 1 ]; then
    ensure_user_exists 'cpr_keykeeper' 'CardProxy.KeyKeeper'
    enable_nginx_config card_proxy_keykeeper2
    register_autostart card_proxy_keykeeper2
fi
#perform_service_restart card_proxy_keykeeper2


%preun keykeeper2
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 0 ]; then
    disable_nginx_config card_proxy_keykeeper2
    service card_proxy_keykeeper2 stop
    unregister_autostart card_proxy_keykeeper2
fi


%files keykeeper2
%defattr(-,root,root)
%{_bindir}/card_proxy_keykeeper2*
%{_sysconfdir}/init.d/card_proxy_keykeeper2
%dir %{_sysconfdir}/card_proxy_keykeeper2
%config(noreplace) %{_sysconfdir}/card_proxy_keykeeper2/card_proxy_keykeeper2.cfg.xml
%config(noreplace) %{_sysconfdir}/nginx/sites-available/10-card_proxy_keykeeper2


%post confpatch
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 1 ]; then
    ensure_user_exists 'cpr_confpatch' 'CardProxy.ConfPatch'
    enable_nginx_config card_proxy_confpatch
    register_autostart card_proxy_confpatch
fi
#perform_service_restart card_proxy_confpatch


%preun confpatch
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 0 ]; then
    disable_nginx_config card_proxy_confpatch
    service card_proxy_confpatch stop
    unregister_autostart card_proxy_confpatch
fi


%files confpatch
%defattr(-,root,root)
%{_bindir}/card_proxy_confpatch*
%{_sysconfdir}/init.d/card_proxy_confpatch
%dir %{_sysconfdir}/card_proxy_confpatch
%config(noreplace) %{_sysconfdir}/card_proxy_confpatch/card_proxy_confpatch.cfg.xml
%config(noreplace) %{_sysconfdir}/nginx/sites-available/10-card_proxy_confpatch
%config %{_sysconfdir}/sudoers.d/card_proxy_confpatch


%post keyapi
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 1 ]; then
    ensure_user_exists 'cpr_keyapi' 'CardProxy.KeyAPI'
    enable_nginx_config card_proxy_keyapi
    register_autostart card_proxy_keyapi
fi
#perform_service_restart card_proxy_keyapi


%preun keyapi
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 0 ]; then
    disable_nginx_config card_proxy_keyapi
    service card_proxy_keyapi stop
    unregister_autostart card_proxy_keyapi
fi


%files keyapi
%defattr(-,root,root)
%{_bindir}/card_proxy_keyapi*
%{_sysconfdir}/init.d/card_proxy_keyapi
%dir %{_sysconfdir}/card_proxy_keyapi
%config(noreplace) %{_sysconfdir}/card_proxy_keyapi/card_proxy_keyapi.cfg.xml
%config(noreplace) %{_sysconfdir}/nginx/sites-available/10-card_proxy_keyapi


%post secvault
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 1 ]; then
    ensure_user_exists 'cpr_secvault' 'CardProxy.SecureVault'
    enable_nginx_config card_proxy_secvault
    register_autostart card_proxy_secvault
fi
#perform_service_restart card_proxy_secvault


%preun secvault
. /usr/share/card_proxy_service/functions.sh
if [ $1 = 0 ]; then
    disable_nginx_config card_proxy_secvault
    service card_proxy_secvault stop
    unregister_autostart card_proxy_secvault
fi


%files secvault
%defattr(-,root,root)
%{_bindir}/card_proxy_secvault*
%{_sysconfdir}/init.d/card_proxy_secvault
%dir %{_sysconfdir}/card_proxy_secvault
%config(noreplace) %{_sysconfdir}/card_proxy_secvault/card_proxy_secvault.cfg.xml
%config(noreplace) %{_sysconfdir}/nginx/sites-available/10-card_proxy_secvault


%files tests
%defattr(-,root,root)
/usr/share/card_proxy_tests/*


%files keyman
%defattr(-,root,root)
%{_bindir}/card_proxy_keyman


%changelog
* Mon May 16 2016 Viacheslav Naydenov <vaclav@yandex.ru> - 0.5.7-2
- Force linking in VaultUser class

* Sun May 15 2016 Viacheslav Naydenov <vaclav@yandex.ru> - 0.5.7-1
- first build of RPM package
