FROM fedora:36

RUN dnf update -y
RUN dnf install -y @development-tools autoconf automake libtool 'dnf-command(config-manager)'

RUN dnf install -y mingw{32,64}-{gcc-c++,gtk2,libxml2,minizip,fftw,libwebp,OpenEXR}

RUN mkdir gwy-z-module

COPY . gwy-z-module
WORKDIR "/gwy-z-module"

RUN dnf install -y ./gwyddion-release-36-1.fc36.noarch.rpm
RUN dnf config-manager --set-enabled gwyddion-mingw
RUN dnf install -y gwyddion-devel mingw32-gwyddion-libs
