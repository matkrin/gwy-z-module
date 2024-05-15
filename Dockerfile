FROM fedora:39

RUN dnf update -y
RUN dnf install -y @development-tools

RUN mkdir gwy-modules

COPY . gwy-modules
WORKDIR "/gwy-modules"

RUN dnf install -y gwyddion-release-39-1.fc39.noarch.rpm
RUN dnf install -y gwyddion-devel
