# Projecteur Linux Repositories

This document aims to list all Linux repositories where _Projecteur_ is available.

Is something missing? Please let me know or create a pull request.

## Official Repositories

### Debian (and Debian based distributions)

The stable version of _Projecteur_ is available in Debian starting
with _Debian bullseye_.
See [this listing](https://packages.debian.org/search?keywords=projecteur&searchon=names&suite=all&section=all)
for all available `projecteur` packages in Debian.

### Ubuntu

Thanks to debian packages, _Projecteur_ is availabed in the official Ubuntu repositories
from Ubuntu 20.10 on. See: https://packages.ubuntu.com/search?keywords=projecteur&searchon=names

### Gentoo Linux

See: https://packages.gentoo.org/packages/x11-misc/projecteur

## User Repositories

### Arch Linux

* https://aur.archlinux.org/packages/projecteur/
* https://aur.archlinux.org/packages/projecteur-git/

### OpenSUSE

User/community repositories:
* https://software.opensuse.org/package/projecteur?search_term=projecteur

### Projecteur's Development Repositories

Automated project builds from the development branch of _Projecteur_ are also
uploaded to [cloudsmith.io](https://cloudsmith.io/~jahnf/repos/projecteur-develop/packages/)
and are accessible as a Linux repository for different distributions.

See also:
 * https://cloudsmith.io/~jahnf/repos/projecteur-develop/setup/#formats-deb
 * https://cloudsmith.io/~jahnf/repos/projecteur-develop/setup/#formats-rpm
 
[![Cloudsmith OSS Hosting](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=for-the-badge)](https://cloudsmith.com)

#### Debian Stretch

```
apt-get install -y debian-keyring
apt-get install -y debian-archive-keyring
apt-get install -y apt-transport-https
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key' | apt-key add -
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.deb.txt?distro=debian&codename=stretch' > /etc/apt/sources.list.d/jahnf-projecteur-develop.list
apt-get update
```

#### Debian Buster

```
apt-get install -y debian-keyring
apt-get install -y debian-archive-keyring
apt-get install -y apt-transport-https
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key' | apt-key add -
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.deb.txt?distro=debian&codename=buster' > /etc/apt/sources.list.d/jahnf-projecteur-develop.list
apt-get update
```

#### Ubuntu 18.04

```
apt-get install -y apt-transport-https
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key' | apt-key add -
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.deb.txt?distro=ubuntu&codename=bionic' > /etc/apt/sources.list.d/jahnf-projecteur-develop.list
apt-get update
```

#### Ubuntu 20.04

```
apt-get install -y apt-transport-https
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key' | apt-key add -
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.deb.txt?distro=ubuntu&codename=focal' > /etc/apt/sources.list.d/jahnf-projecteur-develop.list
apt-get update
```

#### OpenSuse 15.1

```
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.rpm.txt?distro=opensuse&codename=15.1' > /tmp/jahnf-projecteur-develop.repo
zypper ar -f '/tmp/jahnf-projecteur-develop.repo'
zypper --gpg-auto-import-keys refresh jahnf-projecteur-develop jahnf-projecteur-develop-source
```

#### OpenSuse 15.2

```
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.rpm.txt?distro=opensuse&codename=15.2' > /tmp/jahnf-projecteur-develop.repo
zypper ar -f '/tmp/jahnf-projecteur-develop.repo'
zypper --gpg-auto-import-keys refresh jahnf-projecteur-develop jahnf-projecteur-develop-source
```

#### Fedora 31

 ```
dnf install yum-utils pygpgme
rpm --import 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key'
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.rpm.txt?distro=fedora&codename=31' > /tmp/jahnf-projecteur-develop.repo
dnf config-manager --add-repo '/tmp/jahnf-projecteur-develop.repo'
dnf -q makecache -y --disablerepo='*' --enablerepo='jahnf-projecteur-develop' --enablerepo='jahnf-projecteur-develop-source'
```

#### Fedora 32

```
dnf install yum-utils pygpgme
rpm --import 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key'
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.rpm.txt?distro=fedora&codename=32' > /tmp/jahnf-projecteur-develop.repo
dnf config-manager --add-repo '/tmp/jahnf-projecteur-develop.repo'
dnf -q makecache -y --disablerepo='*' --enablerepo='jahnf-projecteur-develop' --enablerepo='jahnf-projecteur-develop-source'
```

#### Fedora 33

```
dnf install yum-utils pygpgme
rpm --import 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key'
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.rpm.txt?distro=fedora&codename=33' > /tmp/jahnf-projecteur-develop.repo
dnf config-manager --add-repo '/tmp/jahnf-projecteur-develop.repo'
dnf -q makecache -y --disablerepo='*' --enablerepo='jahnf-projecteur-develop' --enablerepo='jahnf-projecteur-develop-source'
```

#### CentOS 8

```
yum install yum-utils pygpgme
rpm --import 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/gpg/gpg.544E6934C0570750.key'
curl -1sLf 'https://dl.cloudsmith.io/public/jahnf/projecteur-develop/cfg/setup/config.rpm.txt?distro=el&codename=8' > /tmp/jahnf-projecteur-develop.repo
yum-config-manager --add-repo '/tmp/jahnf-projecteur-develop.repo'
yum -q makecache -y --disablerepo='*' --enablerepo='jahnf-projecteur-develop'
```
