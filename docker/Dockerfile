### This dockerfile sets up an Ubuntu 18.04 environment from scratch 
### that is sufficient to *run* Millipede (but not to build it).

### Note: Dockerfile best practices include:
###  * Combine apt-get update and install into the same RUN line.
###  * Always use "apt-get" and not "apt", because "apt" is for an interactive CLI view.
###  * Always use "apt-get install -y" to ensure that "Yes" is auto-chosen for installation.

FROM ubuntu:18.04
LABEL author=t-keboo@microsoft.com


# Basic set up for ubuntu image
# Remove some warnings about deb pkg manager
# See the following links:
# * https://github.com/phusion/baseimage-docker/issues/58
# * https://github.com/phusion/baseimage-docker/issues/319#issuecomment-245857919
ENV DEBIAN_FRONTEND noninteractive
RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y --no-install-recommends apt-utils gnupg
# RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections

# Just for convenience purposes
RUN apt-get install -y vim

# Required for downloading and building the source packages belowo
RUN apt-get -y install -y g++ cmake make wget git

# Needed to build and run Millipede. 
# Currently I'm not sure if *all* of these are needed to *run* Millipede, but some are. 
RUN apt-get -y install -y \
    libgflags-dev \
    liblapack-dev \
    libblas-dev \
    libboost-all-dev \
    # doxygen \
    nlohmann-json-dev \
    python-numpy \
    python-pyqt5 \
    libnuma-dev \
    libpython-dev \
    swig


# Install gtest and gflags
RUN apt-get install -y libgtest-dev
RUN (cd /usr/src/gtest && cmake . && make && mv libg* /usr/lib/)


# Build and install armadillo
RUN wget http://sourceforge.net/projects/arma/files/armadillo-9.300.2.tar.xz
RUN tar xf armadillo-9.300.2.tar.xz
RUN (cd armadillo-9.300.2; cmake .; make -j8; make install)
RUN rm -rf armadillo*


# Set up Intel MKL
RUN apt-get install -y apt-transport-https ca-certificates
RUN ( \
    cd /tmp; \
    wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB; \
    apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB; \
    rm GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB; \
    sh -c 'echo deb https://apt.repos.intel.com/mkl all main > /etc/apt/sources.list.d/intel-mkl.list'; \
)
RUN apt-get update && apt-get install -y intel-mkl-64bit-2020.0-088


# Set up SoapySDR
RUN ( \
    cd `mktemp -d`; \
    git clone --depth=1 https://github.com/pothosware/SoapySDR.git; \
    cd SoapySDR; \
    mkdir build; \
    cd build; \
    cmake ..; \
    make -j8; \
    make install; \
    ldconfig; \
)

# Undo the noninteractive DEBIAN_FRONTEND from the beginning of this file
ENV DEBIAN_FRONTEND teletype


# Copy in our relevant build files. ("test" is optional)
COPY build/  Millipede/build/
COPY data/   Millipede/data/
COPY test/   Millipede/test/
COPY run_millipede.sh Millipede/

# Copy the FlexRAN files
COPY FlexRAN-FEC-SDK-19-04/ /opt/

# Set up the intel MKL compiler variables (shell environment vars) so that Millipede can run.
# This is only really needed when logging into the container via a remote bash shell and manually running commands.
# Note that this will NOT take effect if a docker CMD or kubernetes command is used directly.
RUN echo "source /opt/intel/compilers_and_libraries/linux/bin/compilervars.sh intel64" >> ~/.bashrc


# Finally, do something, e.g., run Millipede.
# TODO, e.g., CMD ./build/millipede data/tddconfig-sim-ul.json

# This command is for debugging/testing; it makes the container run forever so we can remote into it.
# It's better to do this using a kubernetes command instead.
# CMD exec /bin/bash -c "trap : TERM INT; sleep infinity & wait"