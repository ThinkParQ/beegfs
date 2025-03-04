%define VER %(echo '%{BEEGFS_VERSION}' | cut -d - -f 1)

%define BEEGFS_MAJOR_VERSION %(echo '%{BEEGFS_VERSION}' | cut -d . -f 1)
%define CLIENT_DIR /opt/beegfs/src/client/client_module_%{BEEGFS_MAJOR_VERSION}
%define CLIENT_COMPAT_DIR /opt/beegfs/src/client/client_compat_module_%{BEEGFS_MAJOR_VERSION}

%define is_sles %(test -f /etc/SUSEConnect && echo 1 || echo 0)

%if %is_sles
%define distver %(release="`rpm -qf --queryformat='%%{VERSION}' /etc/os-release 2> /dev/null | tr . : | sed s/:.*$//g`" ; if test $? != 0 ; then release="" ; fi ; echo "$release")
%define RELEASE sles%{distver}

%else
%if %{defined ?dist}
%define RELEASE %(tr -d . <<< %{?dist})

%else
%define RELEASE generic

%endif
%endif

%define FULL_VERSION %{EPOCH}:%{VER}-%{RELEASE}

%define post_package() if [ "$1" = 1 ] \
then \
	output=$(systemctl is-system-running 2> /dev/null) \
	if [ "$?" == 127 ] \
	then \
		chkconfig %1 on \
	elif [ "$?" == 0 ] || ( [ "$output" != "offline" ] && [ "$output" != "unknown" ] ) \
	then \
		systemctl enable %1.service \
	else \
		chkconfig %1 on \
	fi \
fi

%define preun_package() if [ "$1" = 0 ] \
then \
	output=$(systemctl is-system-running 2> /dev/null) \
	if [ "$?" == 127 ] \
	then \
		chkconfig %1 off \
	elif [ "$?" == 0 ] || ( [ "$output" != "offline" ] && [ "$output" != "unknown" ] ) \
	then \
		systemctl disable %1.service \
	else \
		chkconfig %1 off \
	fi \
fi


Name: beegfs
Summary: BeeGFS parallel file system
License: BeeGFS EULA
Version: %{VER}
Release: %{RELEASE}
URL: http://www.beegfs.io
Source: beegfs-%{BEEGFS_VERSION}.tar
Vendor: ThinkParQ GmbH
BuildRoot: %{_tmppath}/beegfs-root
Epoch: %{EPOCH}

%description

Distribution of the BeeGFS parallel filesystem.

%clean
rm -rf %{buildroot}

%prep
%setup -c

%define make_j %{?MAKE_CONCURRENCY:-j %{MAKE_CONCURRENCY}}

%build

export BEEGFS_VERSION=%{BEEGFS_VERSION}
export WITHOUT_COMM_DEBUG=1
export BEEGFS_NVFS=1

make %make_j daemons utils

# disabled, doesn't build
#make -C beeond_thirdparty/build %make_j
make -C beeond_thirdparty_gpl/build %make_j
make -C utils/build %make_j java_lib

%install

export BEEGFS_VERSION=%{BEEGFS_VERSION}
export WITHOUT_COMM_DEBUG=1


# makefiles need some adjustments still
mkdir -p \
   ${RPM_BUILD_ROOT}/opt/beegfs/sbin \
   ${RPM_BUILD_ROOT}/opt/beegfs/lib \
   ${RPM_BUILD_ROOT}/usr/bin \
   ${RPM_BUILD_ROOT}/usr/include \
   ${RPM_BUILD_ROOT}/sbin \
   ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-client-devel/examples/ \
   ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-utils-devel/examples/beegfs-event-listener/build \
   ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-utils-devel/examples/beegfs-event-listener/source \
   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/parallel \
   ${RPM_BUILD_ROOT}/etc/bash_completion.d

##########
########## common files
##########

install -D common_package/scripts/etc/beegfs/lib/start-stop-functions \
   ${RPM_BUILD_ROOT}/etc/beegfs/lib/start-stop-functions
install -D common_package/scripts/etc/beegfs/lib/init-multi-mode \
   ${RPM_BUILD_ROOT}/etc/beegfs/lib/init-multi-mode

##########
########## libbeegfs-ib files
##########

install -D common/build/libbeegfs_ib.so \
   ${RPM_BUILD_ROOT}/opt/beegfs/lib/libbeegfs_ib.so

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/libbeegfs-ib/copyright

##########
########## daemons, utils
##########

make DESTDIR=${RPM_BUILD_ROOT} daemons-install utils-install

##########
########## common directories for extra files
##########
mkdir -p \
   ${RPM_BUILD_ROOT}/etc/beegfs \
   ${RPM_BUILD_ROOT}/etc/init.d \
   ${RPM_BUILD_ROOT}/opt/beegfs/scripts/grafana

##########
########## meta extra files
##########

cp -a meta/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs

# we use the redhat script for all rpm distros, as we now provide our own
# daemon() and killproc() function library (derived from redhat)
install -D meta/build/dist/etc/init.d/beegfs-meta.init \
   ${RPM_BUILD_ROOT}/etc/init.d/beegfs-meta

#add the genric part of the init script from the common package
cat common_package/build/dist/etc/init.d/beegfs-service.init >> ${RPM_BUILD_ROOT}/etc/init.d/beegfs-meta

#install systemd unit description
install -D -m644 meta/build/dist/usr/lib/systemd/system/beegfs-meta.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-meta.service
install -D -m644 meta/build/dist/usr/lib/systemd/system/beegfs-meta@.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-meta@.service

install -D meta/build/dist/sbin/beegfs-setup-meta \
	${RPM_BUILD_ROOT}/opt/beegfs/sbin/beegfs-setup-meta

install -D meta/build/dist/etc/default/beegfs-meta ${RPM_BUILD_ROOT}/etc/default/beegfs-meta

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-meta/copyright

##########
########## mgmtd extra files
##########

cp -a mgmtd/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs/

# we use the redhat script for all rpm distros, as we now provide our own
# daemon() and killproc() function library (derived from redhat)
install -D mgmtd/build/dist/etc/init.d/beegfs-mgmtd.init \
   ${RPM_BUILD_ROOT}/etc/init.d/beegfs-mgmtd

#add the genric part of the init script from the common package
cat common_package/build/dist/etc/init.d/beegfs-service.init >> ${RPM_BUILD_ROOT}/etc/init.d/beegfs-mgmtd

#install systemd unit description
install -D -m644 mgmtd/build/dist/usr/lib/systemd/system/beegfs-mgmtd.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-mgmtd.service
install -D -m644 mgmtd/build/dist/usr/lib/systemd/system/beegfs-mgmtd@.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-mgmtd@.service

install -D mgmtd/build/dist/sbin/beegfs-setup-mgmtd \
	${RPM_BUILD_ROOT}/opt/beegfs/sbin/beegfs-setup-mgmtd

install -D mgmtd/build/dist/etc/default/beegfs-mgmtd ${RPM_BUILD_ROOT}/etc/default/beegfs-mgmtd

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-mgmtd/copyright

##########
########## storage extra files
##########

cp -a storage/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs/
mkdir -p ${RPM_BUILD_ROOT}/etc/init.d/

# we use the redhat script for all rpm distros, as we now provide our own
# daemon() and killproc() function library (derived from redhat)
install -D storage/build/dist/etc/init.d/beegfs-storage.init \
   ${RPM_BUILD_ROOT}/etc/init.d/beegfs-storage

#add the genric part of the init script from the common package
cat common_package/build/dist/etc/init.d/beegfs-service.init >> ${RPM_BUILD_ROOT}/etc/init.d/beegfs-storage

#install systemd unit description
install -D -m644 storage/build/dist/usr/lib/systemd/system/beegfs-storage.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-storage.service
install -D -m644 storage/build/dist/usr/lib/systemd/system/beegfs-storage@.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-storage@.service

install -D storage/build/dist/sbin/beegfs-setup-storage \
	${RPM_BUILD_ROOT}/opt/beegfs/sbin/beegfs-setup-storage

install -D storage/build/dist/etc/default/beegfs-storage ${RPM_BUILD_ROOT}/etc/default/beegfs-storage

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-storage/copyright

##########
########## helperd extra files
##########

cp -a helperd/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs/

# we use the redhat script for all rpm distros, as we now provide our own
# daemon() and killproc() function library (derived from redhat)
install -D helperd/build/dist/etc/init.d/beegfs-helperd.init \
   ${RPM_BUILD_ROOT}/etc/init.d/beegfs-helperd

#add the genric part of the init script from the common package
cat common_package/build/dist/etc/init.d/beegfs-service.init >> ${RPM_BUILD_ROOT}/etc/init.d/beegfs-helperd

#install systemd unit description
install -D -m644 helperd/build/dist/usr/lib/systemd/system/beegfs-helperd.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-helperd.service
install -D -m644 helperd/build/dist/usr/lib/systemd/system/beegfs-helperd@.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-helperd@.service

install -D helperd/build/dist/etc/default/beegfs-helperd ${RPM_BUILD_ROOT}/etc/default/beegfs-helperd

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-helperd/copyright

##########
########## mon extra files
##########

install -D -m644 mon/build/dist/etc/beegfs-mon.conf ${RPM_BUILD_ROOT}/etc/beegfs
install -D -m600 mon/build/dist/etc/beegfs-mon.auth ${RPM_BUILD_ROOT}/etc/beegfs

# we use the redhat script for all rpm distros, as we now provide our own
# daemon() and killproc() function library (derived from redhat)
install -D mon/build/dist/etc/init.d/beegfs-mon.init \
   ${RPM_BUILD_ROOT}/etc/init.d/beegfs-mon

#add the genric part of the init script from the common package
cat common_package/build/dist/etc/init.d/beegfs-service.init >> ${RPM_BUILD_ROOT}/etc/init.d/beegfs-mon

#install systemd unit description
install -D -m644 mon/build/dist/usr/lib/systemd/system/beegfs-mon.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-mon.service
install -D -m644 mon/build/dist/usr/lib/systemd/system/beegfs-mon@.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-mon@.service

install -D mon/build/dist/etc/default/beegfs-mon ${RPM_BUILD_ROOT}/etc/default/beegfs-mon
cp -a mon/scripts/grafana/* ${RPM_BUILD_ROOT}/opt/beegfs/scripts/grafana/

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-mon/copyright

##########
########## mon-grafana
##########

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-mon-grafana/copyright

##########
########## utils
##########

cp -a utils/scripts/fsck.beegfs ${RPM_BUILD_ROOT}/sbin/

ln -s /opt/beegfs/sbin/beegfs-ctl ${RPM_BUILD_ROOT}/usr/bin/beegfs-ctl
ln -s /opt/beegfs/sbin/beegfs-fsck ${RPM_BUILD_ROOT}/usr/bin/beegfs-fsck

cp -a utils/scripts/beegfs-* ${RPM_BUILD_ROOT}/usr/bin/
cp utils/build/libjbeegfs.so utils/build/jbeegfs.jar \
   ${RPM_BUILD_ROOT}/opt/beegfs/lib/
cp -a utils/scripts/etc/bash_completion.d/beegfs-ctl \
   ${RPM_BUILD_ROOT}/etc/bash_completion.d/

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-utils/copyright


##########
########## utils-devel
##########

cp -a event_listener/include/* ${RPM_BUILD_ROOT}/usr/include/
cp -a event_listener/build/Makefile \
   ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-utils-devel/examples/beegfs-event-listener/build/
cp -a event_listener/source/beegfs-event-listener.cpp \
   ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-utils-devel/examples/beegfs-event-listener/source/

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-utils-devel/copyright

##########
########## eventlistener extra files
##########
# copying event-listener config file
cp -a event_listener/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs

install -D event_listener/build/dist/etc/init.d/beegfs-eventlistener.init \
   ${RPM_BUILD_ROOT}/etc/init.d/beegfs-eventlistener

#install systemd unit description
install -D -m644 event_listener/build/dist/usr/lib/systemd/system/beegfs-eventlistener.service \
	${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-eventlistener.service

##########
########## client
##########

make -C client_module/build %make_j \
   RELEASE_PATH=${RPM_BUILD_ROOT}/opt/beegfs/src/client KDIR="%{KDIR}" V=1 \
   prepare_release
cp client_module/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs/
cp client_module/build/dist/etc/beegfs-client-build.mk ${RPM_BUILD_ROOT}/etc/beegfs/beegfs-client-build.mk


# compat files
cp -a ${RPM_BUILD_ROOT}/%{CLIENT_DIR} ${RPM_BUILD_ROOT}/%{CLIENT_COMPAT_DIR}

echo beegfs-%{BEEGFS_MAJOR_VERSION} | tr -d . > ${RPM_BUILD_ROOT}/%{CLIENT_COMPAT_DIR}/build/beegfs.fstype

# we use the redhat script for all rpm distros, as we now provide our own
# daemon() and killproc() function library (derived from redhat)
install -D client_module/build/dist/etc/init.d/beegfs-client.init ${RPM_BUILD_ROOT}/etc/init.d/beegfs-client

#install systemd unit description
install -D -m644 client_module/build/dist/usr/lib/systemd/system/beegfs-client.service \
   ${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-client.service

install -D -m644 client_module/build/dist/usr/lib/systemd/system/beegfs-client@.service \
   ${RPM_BUILD_ROOT}/usr/lib/systemd/system/beegfs-client@.service


install -D client_module/build/dist/etc/default/beegfs-client ${RPM_BUILD_ROOT}/etc/default/beegfs-client

install -D client_module/scripts/etc/beegfs/lib/init-multi-mode.beegfs-client \
   ${RPM_BUILD_ROOT}/etc/beegfs/lib/init-multi-mode.beegfs-client

install -D client_module/build/dist/sbin/beegfs-setup-client \
   ${RPM_BUILD_ROOT}/opt/beegfs/sbin/beegfs-setup-client

install -D client_module/build/dist/etc/beegfs-client-mount-hook.example \
   ${RPM_BUILD_ROOT}/etc/beegfs/beegfs-client-mount-hook.example

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-client/copyright

##########
########## client-dkms
##########

cp client_module/build/dist/etc/*.conf ${RPM_BUILD_ROOT}/etc/beegfs/

mkdir -p ${RPM_BUILD_ROOT}/usr/src/beegfs-%{VER}

cp -r client_module/build ${RPM_BUILD_ROOT}/usr/src/beegfs-%{VER}
cp -r client_module/source ${RPM_BUILD_ROOT}/usr/src/beegfs-%{VER}
cp -r client_module/include ${RPM_BUILD_ROOT}/usr/src/beegfs-%{VER}

rm -Rf ${RPM_BUILD_ROOT}/usr/src/beegfs-%{VER}/build/dist


install -D client_module/build/dist/sbin/beegfs-setup-client \
   ${RPM_BUILD_ROOT}/opt/beegfs/sbin/beegfs-setup-client

sed -e 's/__VERSION__/%{VER}/g' -e 's/__NAME__/beegfs/g' -e 's/__MODNAME__/beegfs/g' \
	 < client_module/dkms.conf.in \
	 > ${RPM_BUILD_ROOT}/usr/src/beegfs-%{VER}/dkms.conf

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-client-dkms/copyright

##########
########## client-compat
##########

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-client-compat/copyright

##########
########## client-devel
##########

cp -a client_devel/include/beegfs \
   ${RPM_BUILD_ROOT}/usr/include/
cp -a client_module/include/uapi/* \
   ${RPM_BUILD_ROOT}/usr/include/beegfs/
sed -i '~s~uapi/beegfs_client~beegfs/beegfs_client~g' \
   ${RPM_BUILD_ROOT}/usr/include/beegfs/*.h
cp -a client_devel/build/dist/usr/share/doc/beegfs-client-devel/examples/* \
   ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-client-devel/examples/

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beegfs-client-devel/copyright

##########
########## beeond
##########

install -D beeond/source/beeond ${RPM_BUILD_ROOT}/opt/beegfs/sbin/beeond
install -D beeond/source/beeond-cp ${RPM_BUILD_ROOT}/opt/beegfs/sbin/beeond-cp
cp beeond/scripts/lib/* ${RPM_BUILD_ROOT}/opt/beegfs/lib/
ln -s /opt/beegfs/sbin/beeond ${RPM_BUILD_ROOT}/usr/bin/beeond
ln -s /opt/beegfs/sbin/beeond-cp ${RPM_BUILD_ROOT}/usr/bin/beeond-cp

install -m 644 -D debian/copyright ${RPM_BUILD_ROOT}/usr/share/doc/beeond/copyright

# disabled, doesn't build
###########
########### beeond-thirdparty
###########
#
#install -D beeond_thirdparty/build/pcopy/pcp.bin \
#   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/pcopy
#install -D beeond_thirdparty/build/pcopy/pcp_cleanup \
#   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/pcopy
#install -D beeond_thirdparty/build/pcopy/pcp_run \
#   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/pcopy
#install -D beeond_thirdparty/build/pcopy/ssh.spawner \
#   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/pcopy
#cp beeond_thirdparty/build/pcopy/README.txt \
#   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/pcopy/

##########
########## beeond-thirdparty-gpl
##########

install -D beeond_thirdparty_gpl/build/parallel \
   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/parallel/
cp beeond_thirdparty_gpl/build/COPYING \
   ${RPM_BUILD_ROOT}/opt/beegfs/thirdparty/parallel/



%package common

Summary: BeeGFS common files
Group: Software/Other
BuildArch: noarch

%description common
The package contains files required by all BeeGFS daemons

%files common
%defattr(-,root,root)
%dir /etc/beegfs/lib/
/etc/beegfs/lib/init-multi-mode
/etc/beegfs/lib/start-stop-functions


%package -n libbeegfs-ib

Summary: BeeGFS InfiniBand support
Group: Software/Other
Buildrequires: librdmacm-devel, libibverbs-devel
BuildRequires: libnl3-devel

%description -n libbeegfs-ib
This package contains support libraries for InfiniBand.

%files -n libbeegfs-ib
%license /usr/share/doc/libbeegfs-ib/copyright
/opt/beegfs/lib/libbeegfs_ib.so



%package meta

Summary: BeeGFS meta server daemon
Group: Software/Other
requires: beegfs-common = %{FULL_VERSION}
BuildRequires: libnl3-devel

%description meta
This package contains the BeeGFS meta server binaries.

%post meta
%post_package beegfs-meta

%preun meta
%preun_package beegfs-meta

%files meta
%defattr(-,root,root)
%license /usr/share/doc/beegfs-meta/copyright
%config(noreplace) /etc/beegfs/beegfs-meta.conf
%config(noreplace) /etc/default/beegfs-meta
/etc/init.d/beegfs-meta
/opt/beegfs/sbin/beegfs-meta
/opt/beegfs/sbin/beegfs-setup-meta
/usr/lib/systemd/system/beegfs-meta.service
/usr/lib/systemd/system/beegfs-meta@.service



%package mgmtd

Summary: BeeGFS management daemon
Group: Software/Other
requires: beegfs-common = %{FULL_VERSION}
BuildRequires: libnl3-devel

%description mgmtd
The package contains the BeeGFS management daemon.

%post mgmtd
%post_package beegfs-mgmtd

%preun mgmtd
%preun_package beegfs-mgmtd

%files mgmtd
%defattr(-,root,root)
%license /usr/share/doc/beegfs-mgmtd/copyright
%config(noreplace) /etc/beegfs/beegfs-mgmtd.conf
%config(noreplace) /etc/default/beegfs-mgmtd
/etc/init.d/beegfs-mgmtd
/opt/beegfs/sbin/beegfs-mgmtd
/opt/beegfs/sbin/beegfs-setup-mgmtd
/usr/lib/systemd/system/beegfs-mgmtd.service
/usr/lib/systemd/system/beegfs-mgmtd@.service



%package storage

Summary: BeeGFS storage server daemon
Group: Software/Other
requires: beegfs-common = %{FULL_VERSION}
BuildRequires: libnl3-devel

%description storage
This package contains the BeeGFS storage server binaries.

%post storage
%post_package beegfs-storage

%preun storage
%preun_package beegfs-storage

%files storage
%defattr(-,root,root)
%license /usr/share/doc/beegfs-storage/copyright
%config(noreplace) /etc/beegfs/beegfs-storage.conf
%config(noreplace) /etc/default/beegfs-storage
/etc/init.d/beegfs-storage
/opt/beegfs/sbin/beegfs-storage
/opt/beegfs/sbin/beegfs-setup-storage
/usr/lib/systemd/system/beegfs-storage.service
/usr/lib/systemd/system/beegfs-storage@.service



%package helperd

Summary: BeeGFS client helper daemon
Group: Software/Other
requires: beegfs-common = %{FULL_VERSION}
BuildRequires: libnl3-devel

%description helperd
The package contains the BeeGFS helper daemon.

%post helperd
%post_package beegfs-helperd

%preun helperd
%preun_package beegfs-helperd

%files helperd
%defattr(-,root,root)
%license /usr/share/doc/beegfs-helperd/copyright
%config(noreplace) /etc/beegfs/beegfs-helperd.conf
%config(noreplace) /etc/default/beegfs-helperd
/etc/init.d/beegfs-helperd
/opt/beegfs/sbin/beegfs-helperd
/usr/lib/systemd/system/beegfs-helperd.service
/usr/lib/systemd/system/beegfs-helperd@.service



%package mon

Summary: BeeGFS mon server daemon
Group: Software/Other
requires: beegfs-common = %{FULL_VERSION}
BuildRequires: libnl3-devel

%description mon
This package contains the BeeGFS mon server binaries.

%post mon
%post_package beegfs-mon

%preun mon
%preun_package beegfs-mon

%files mon
%defattr(-,root,root)
%license /usr/share/doc/beegfs-mon/copyright
%config(noreplace) /etc/beegfs/beegfs-mon.conf
%config(noreplace) /etc/beegfs/beegfs-mon.auth
%config(noreplace) /etc/default/beegfs-mon
/etc/init.d/beegfs-mon
/opt/beegfs/sbin/beegfs-mon
/usr/lib/systemd/system/beegfs-mon.service
/usr/lib/systemd/system/beegfs-mon@.service



%package mon-grafana

Summary: BeeGFS mon dashboards for Grafana
Group: Software/Other
BuildArch: noarch

%description mon-grafana
This package contains the BeeGFS mon dashboards to display monitoring data in Grafana.

The default dashboard setup requires both Grafana, and InfluxDB.

%files mon-grafana
%license /usr/share/doc/beegfs-mon-grafana/copyright
%defattr(-,root,root)
/opt/beegfs/scripts/grafana/



%package utils

Summary: BeeGFS utilities
Group: Software/Other
requires: beegfs-common = %{FULL_VERSION}
BuildRequires: libnl3-devel

%description utils
This package contains BeeGFS utilities.

%files utils
%defattr(-,root,root)
%license /usr/share/doc/beegfs-utils/copyright
%attr(0755, root, root) /opt/beegfs/sbin/beegfs-fsck
%attr(4755, root, root) /opt/beegfs/sbin/beegfs-ctl
/opt/beegfs/lib/jbeegfs.jar
/opt/beegfs/lib/libjbeegfs.so
/usr/bin/beegfs-check-servers
/usr/bin/beegfs-ctl
/usr/bin/beegfs-df
/usr/bin/beegfs-fsck
/usr/bin/beegfs-net
/usr/lib/systemd/system/beegfs-eventlistener.service
%config(noreplace) /etc/beegfs/beegfs-eventlistener.conf
/etc/init.d/beegfs-eventlistener
/etc/bash_completion.d/beegfs-ctl
/sbin/fsck.beegfs
/opt/beegfs/sbin/beegfs-event-listener


%package utils-devel

Summary: BeeGFS utils devel files
Group: Software/Other
BuildArch: noarch

%description utils-devel
This package contains BeeGFS utils development files and examples.

%files utils-devel
%defattr(-,root,root)
%license /usr/share/doc/beegfs-utils-devel/copyright
/usr/include/beegfs/beegfs_file_event_log.hpp
/usr/share/doc/beegfs-utils-devel/examples/beegfs-event-listener/*



%package client

Summary: BeeGFS client kernel module
License: GPL v2
Group: Software/Other
BuildArch: noarch
%if %is_sles
requires: make, gcc, kernel-default-devel, elfutils
%else
requires: make, gcc
recommends: kernel-devel, elfutils-libelf-devel
%endif
conflicts: beegfs-client-dkms

%description client
This package contains scripts, config and source files to build and
start beegfs-client.

%post client
%post_package beegfs-client

# make the script to run autobuild
mkdir -p /var/lib/beegfs/client
touch /var/lib/beegfs/client/force-auto-build

%preun client
%preun_package beegfs-client

%files client
%defattr(-,root,root)
%license /usr/share/doc/beegfs-client/copyright
%config(noreplace) /etc/beegfs/beegfs-client-autobuild.conf
%config(noreplace) /etc/beegfs/beegfs-client-mount-hook.example
%config(noreplace) /etc/beegfs/beegfs-client.conf
%config(noreplace) /etc/beegfs/beegfs-mounts.conf
%dir /etc/beegfs/lib/
%config(noreplace) /etc/beegfs/lib/init-multi-mode.beegfs-client
%config(noreplace) /etc/default/beegfs-client
/etc/init.d/beegfs-client
/opt/beegfs/sbin/beegfs-setup-client
/usr/lib/systemd/system/beegfs-client.service
/usr/lib/systemd/system/beegfs-client@.service
%{CLIENT_DIR}

%postun client
rm -rf /lib/modules/*/updates/fs/beegfs_autobuild

%package client-dkms

Summary: BeeGFS client kernel module (DKMS version)
License: GPL v2
Group: Software/Other
BuildArch: noarch
%if %is_sles
requires: make, dkms, kernel-default-devel, elfutils
%else
requires: make, dkms
recommends: kernel-devel, elfutils-libelf-devel
%endif
conflicts: beegfs-client
provides: beegfs-client

%description client-dkms
This package contains scripts, config and source files to build and
start beegfs-client. It uses DKMS to build the kernel module.

%post client-dkms
dkms install beegfs/%{VER}

%preun client-dkms
dkms remove beegfs/%{VER} --all

%files client-dkms
%defattr(-,root,root)
%license /usr/share/doc/beegfs-client-dkms/copyright
%config(noreplace) /etc/beegfs/beegfs-client.conf
%config(noreplace) /etc/beegfs/beegfs-client-build.mk
/usr/src/beegfs-%{VER}



%package client-compat

Summary: BeeGFS client compat module, allows to run two different client versions.
License: GPL v2
Group: Software/Other
%if %is_sles
requires: make, gcc, kernel-default-devel, elfutils
%else
requires: make, gcc
recommends: kernel-devel, elfutils-libelf-devel
%endif
BuildArch: noarch

%description client-compat
This package allows to build and to run a compatbility beegfs-client kernel module
on a system that has a newer beegfs-client version installed.

%files client-compat
%license /usr/share/doc/beegfs-client-compat/copyright
%defattr(-,root,root)
%{CLIENT_COMPAT_DIR}



%package client-devel

Summary: BeeGFS client devel files
Group: Software/Other
BuildArch: noarch

%description client-devel
This package contains BeeGFS client development files.

%files client-devel
%defattr(-,root,root)
%license /usr/share/doc/beegfs-client-devel/copyright
%dir /usr/include/beegfs
/usr/include/beegfs/beegfs.h
/usr/include/beegfs/beegfs_client.h
/usr/include/beegfs/beegfs_ioctl.h
/usr/include/beegfs/beegfs_ioctl_functions.h
/usr/share/doc/beegfs-client-devel/examples/createFileWithStripePattern.cpp
/usr/share/doc/beegfs-client-devel/examples/getStripePatternOfFile.cpp



%package -n beeond

Summary: BeeOND
Group: Software/Other
requires: beeond-thirdparty-gpl = %{FULL_VERSION}, beegfs-utils = %{FULL_VERSION}, beegfs-mgmtd = %{FULL_VERSION}, beegfs-meta = %{FULL_VERSION}, beegfs-storage = %{FULL_VERSION}, beegfs-client = %{FULL_VERSION}, beegfs-helperd = %{FULL_VERSION}, psmisc
BuildArch: noarch

%description -n beeond
This package contains BeeOND.

%files -n beeond
%defattr(-,root,root)
%license /usr/share/doc/beeond/copyright
/opt/beegfs/sbin/beeond
/usr/bin/beeond
/opt/beegfs/sbin/beeond-cp
/usr/bin/beeond-cp
/opt/beegfs/lib/beeond-lib
/opt/beegfs/lib/beegfs-ondemand-stoplocal



## disabled, doesn't build
#%package -n beeond-thirdparty
#
#Summary: BeeOND Thirdparty
#Group: Software/Other
#
#%description -n beeond-thirdparty
#This package contains thirdparty software needed to run BeeOND.
#
#%files -n beeond-thirdparty
#%defattr(-,root,root)
#/opt/beegfs/thirdparty/pcopy/*



%package -n beeond-thirdparty-gpl

Summary: BeeOND Thirdparty GPL
Group: Software/Other

%description -n beeond-thirdparty-gpl
This package contains thirdparty software (licensed under GPL) needed to run BeeOND.

%files -n beeond-thirdparty-gpl
%defattr(-,root,root)
/opt/beegfs/thirdparty/parallel/parallel
/opt/beegfs/thirdparty/parallel/COPYING
