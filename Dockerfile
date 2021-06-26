FROM voidlinux/voidlinux-musl as loat_build
LABEL author="Florian Frohn"

RUN xbps-install -ySu
RUN xbps-install -y gcc git automake autoconf make cmake lzip wget gperf libtool readline-devel cln-devel pkg-config boost-devel giac-devel python-devel

RUN mkdir /src/

# z3
WORKDIR /src
RUN wget https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.8.10.tar.gz
RUN tar xf z3-4.8.10.tar.gz
WORKDIR /src/z3-z3-4.8.10
RUN mkdir build
WORKDIR /src/z3-z3-4.8.10/build
RUN cmake -DZ3_BUILD_LIBZ3_SHARED=FALSE -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-march=sandybridge" ..
RUN make -j
RUN make install

# gmp
WORKDIR /src
RUN wget https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
RUN lzip -d gmp-6.2.1.tar.lz
RUN tar xf gmp-6.2.1.tar
WORKDIR /src/gmp-6.2.1
RUN ./configure ABI=64 CFLAGS="-fPIC" CPPFLAGS="-DPIC" --host=sandybridge-pc-linux-gnu --enable-cxx
RUN make -j
RUN make -j check
RUN make install

# libpoly
WORKDIR /src
RUN git clone https://github.com/SRI-CSL/libpoly.git
WORKDIR /src/libpoly/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_FLAGS_RELEASE="-march=sandybridge" -DCMAKE_CXX_FLAGS_RELEASE="-march=sandybridge" ..
RUN make -j
RUN make install

# cudd
WORKDIR /src
RUN git clone https://github.com/ivmai/cudd.git
WORKDIR /src/cudd
RUN ./configure CFLAGS='-fPIC -march=sandybridge' CXXFLAGS='-fPIC -march=sandybridge'
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
RUN ./configure --enable-mcsat --with-pic-gmp=/usr/local/lib/libgmp.a CFLAGS='-march=sandybridge'
RUN make -j
RUN make -j static-lib static-dist
# RUN make -j check
RUN make install

# ginac
WORKDIR /src
RUN wget https://www.ginac.de/ginac-1.8.0.tar.bz2
RUN tar xf ginac-1.8.0.tar.bz2
WORKDIR /src/ginac-1.8.0
RUN mkdir build
WORKDIR /src/ginac-1.8.0/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=false -DCMAKE_C_FLAGS_RELEASE="-march=sandybridge" -DCMAKE_CXX_FLAGS_RELEASE="-march=sandybridge" ..
RUN make -j
RUN make install

# ntl
WORKDIR /src
RUN wget https://libntl.org/ntl-11.4.4.tar.gz
RUN tar xf ntl-11.4.4.tar.gz
WORKDIR /src/ntl-11.4.4/src
RUN ./configure CXXFLAGS='-march=sandybridge'
RUN make -j
RUN make install

# purrs
WORKDIR /src
RUN git clone https://github.com/aprove-developers/LoAT-purrs.git
WORKDIR /src/LoAT-purrs
RUN autoreconf --install
RUN automake
RUN ./configure CFLAGS='-march=sandybridge' CXXFLAGS='-march=sandybridge'
RUN make -j
RUN make install

# loat
RUN mkdir -p /home/ffrohn/repos/LoAT
WORKDIR /home/ffrohn/repos/LoAT
COPY CMakeLists.txt /home/ffrohn/repos/LoAT/
COPY src /home/ffrohn/repos/LoAT/src/
RUN mkdir -p /home/ffrohn/repos/LoAT/build/static/release
WORKDIR /home/ffrohn/repos/LoAT/build/static/release
RUN cmake -DSTATIC=1 -DCMAKE_BUILD_TYPE=Release ../../../
RUN make -j