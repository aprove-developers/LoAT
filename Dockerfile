FROM voidlinux/voidlinux-musl:20191230 as loat_build
LABEL author="Florian Frohn"

RUN SSL_NO_VERIFY_PEER=1 xbps-install -ySu xbps
RUN SSL_NO_VERIFY_PEER=1 xbps-install -ySu
RUN xbps-install -y gcc
RUN xbps-install -y git
RUN xbps-install -y automake
RUN xbps-install -y autoconf
RUN xbps-install -y make
RUN xbps-install -y cmake
RUN xbps-install -y lzip
RUN xbps-install -y wget
RUN xbps-install -y gperf
RUN xbps-install -y libtool
RUN xbps-install -y readline-devel
RUN xbps-install -y cln-devel
RUN xbps-install -y pkg-config
RUN xbps-install -y boost-devel
RUN xbps-install -y giac-devel
RUN xbps-install -y python-devel

RUN mkdir /src/

# z3
WORKDIR /src
RUN wget https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.8.10.tar.gz
RUN tar xf z3-4.8.10.tar.gz
WORKDIR /src/z3-z3-4.8.10
RUN mkdir build
WORKDIR /src/z3-z3-4.8.10/build
RUN cmake -DZ3_BUILD_LIBZ3_SHARED=FALSE -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-march=sandybridge -O3 -DNDEBUG" ..
RUN make -j
RUN make install

# position-independent gmp
WORKDIR /src
RUN wget https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
RUN lzip -d gmp-6.2.1.tar.lz
RUN tar xf gmp-6.2.1.tar
WORKDIR /src/gmp-6.2.1
RUN ./configure ABI=64 CFLAGS="-fPIC -O3 -DNDEBUG" CPPFLAGS="-DPIC -O3 -DNDEBUG" --host=sandybridge-pc-linux-gnu --enable-cxx --prefix /gmp/
RUN make -j
RUN make -j check
RUN make install

# # gmp
# WORKDIR /src
# RUN rm -rf /src/gmp-6.2.1
# RUN tar xf gmp-6.2.1.tar
# WORKDIR /src/gmp-6.2.1
# RUN ./configure ABI=64 --host=sandybridge-pc-linux-gnu --enable-cxx
# RUN make -j
# RUN make -j check
# RUN make install

RUN xbps-install -y gmp-devel gmpxx-devel

# libpoly
WORKDIR /src
RUN git clone https://github.com/SRI-CSL/libpoly.git
WORKDIR /src/libpoly/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_FLAGS_RELEASE="-march=sandybridge -O3 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-march=sandybridge -O3 -DNDEBUG" ..
RUN make -j
RUN make install

# cudd
WORKDIR /src
RUN git clone https://github.com/ivmai/cudd.git
WORKDIR /src/cudd
# make check fails when compiled with -DNDEBUG
RUN ./configure CFLAGS='-fPIC -march=sandybridge -O3' CXXFLAGS='-fPIC -march=sandybridge -O3'
RUN sed -i 's/aclocal-1.14/aclocal-1.16/g' Makefile
RUN sed -i 's/automake-1.14/automake-1.16/g' Makefile
RUN make -j
RUN make -j check
RUN make install

# yices
WORKDIR /src
RUN git clone https://github.com/SRI-CSL/yices2.git
WORKDIR /src/yices2
RUN autoconf
RUN ./configure --enable-mcsat --with-pic-gmp=/gmp/lib/libgmp.a CFLAGS='-march=sandybridge -O3 -DNDEBUG'
RUN make -j
RUN make -j static-lib static-dist
RUN make install

# ginac
WORKDIR /src
RUN wget https://www.ginac.de/ginac-1.8.3.tar.bz2
RUN tar xf ginac-1.8.3.tar.bz2
WORKDIR /src/ginac-1.8.3
RUN mkdir build
WORKDIR /src/ginac-1.8.3/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=false -DCMAKE_C_FLAGS_RELEASE="-march=sandybridge -O3 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-march=sandybridge -O3 -DNDEBUG" ..
RUN make -j
RUN make install

# ntl
WORKDIR /src
RUN wget https://libntl.org/ntl-11.4.4.tar.gz
RUN tar xf ntl-11.4.4.tar.gz
WORKDIR /src/ntl-11.4.4/src
RUN ./configure CXXFLAGS='-march=sandybridge -O3 -DNDEBUG'
RUN make -j
RUN make install

# purrs
WORKDIR /src
RUN git clone https://github.com/aprove-developers/LoAT-purrs.git
WORKDIR /src/LoAT-purrs
RUN autoreconf --install
RUN automake
RUN ./configure --with-cxxflags='-march=sandybridge -O3 -DNDEBUG'
RUN make -j
RUN make install

ARG SHA
ARG DIRTY

# loat
RUN mkdir -p /home/ffrohn/repos/LoAT
WORKDIR /home/ffrohn/repos/LoAT
COPY CMakeLists.txt /home/ffrohn/repos/LoAT/
COPY src /home/ffrohn/repos/LoAT/src/
RUN mkdir -p /home/ffrohn/repos/LoAT/build/static/release
WORKDIR /home/ffrohn/repos/LoAT/build/static/release
RUN cmake -DSTATIC=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE='-march=sandybridge -O3 -DNDEBUG' -DCMAKE_CXX_FLAGS_RELEASE='-march=sandybridge -O3 -DNDEBUG' -DSHA=$SHA -DDIRTY=$DIRTY ../../../
RUN make -j