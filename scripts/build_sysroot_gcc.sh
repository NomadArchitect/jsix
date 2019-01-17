#!/usr/bin/env bash

TARGET="x86_64-elf"
NASM_VERSION="2.14.02"
GCC_VERSION="7.4.0"
BINUTILS_VERSION="2.31.1"

SYSROOT=$(realpath "$(dirname $0)/../sysroot")
WORK=$(realpath "$(dirname $0)/sysroot")

set -e
mkdir -p "${SYSROOT}"
mkdir -p "${WORK}"


function build_nasm() {
	if [[ ! -d "${WORK}/nasm-${NASM_VERSION}" ]]; then
		echo "Downloading NASM..."
		tarball="nasm-${NASM_VERSION}.tar.gz"
		curl -sSOL "https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VERSION}/${tarball}"
		tar xzf "${tarball}" -C "${WORK}" && rm "${tarball}"
	fi

	mkdir -p "${WORK}/build/nasm"
	pushd "${WORK}/build/nasm"

	echo "Configuring NASM..."
	"${WORK}/nasm-${NASM_VERSION}/configure" \
		--quiet \
		--config-cache \
		--disable-werror \
		--prefix="${SYSROOT}" \
		--srcdir="${WORK}/nasm-${NASM_VERSION}"

	echo "Building NASM..."
	(make -j && make install) > "${WORK}/build/nasm_build.log"
	popd
}

function build_binutils() {
	if [[ ! -d "${WORK}/binutils-${BINUTILS_VERSION}" ]]; then
		echo "Downloading binutils..."
		tarball="binutils-${BINUTILS_VERSION}.tar.gz"
		curl -sSOL "https://ftp.gnu.org/gnu/binutils/${tarball}"
		tar xzf "${tarball}" -C "${WORK}" && rm "${tarball}"
	fi

	mkdir -p "${WORK}/build/binutils"
	pushd "${WORK}/build/binutils"

	echo "Configuring binutils..."
	"${WORK}/binutils-${BINUTILS_VERSION}/configure" \
		--quiet \
		--target="${TARGET}" \
		--prefix="${SYSROOT}" \
		--with-sysroot \
		--disable-nls \
		--disable-werror

	echo "Building binutils..."
	(make -j && make install) > "${WORK}/build/binutils_build.log"
	popd
}

function build_gcc() {
	if [[ ! -d "${WORK}/gcc-${GCC_VERSION}" ]]; then
		echo "Downloading GCC..."
		tarball="gcc-${GCC_VERSION}.tar.gz"
		curl -sSOL "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/${tarball}"
		tar xzf "${tarball}" -C "${WORK}" && rm "${tarball}"

		# no-red-zone support version of libgcc
		echo "MULTILIB_OPTIONS += mno-red-zone" >  "${WORK}/gcc-${GCC_VERSION}/gcc/config/i386/t-${TARGET}"
		echo "MULTILIB_DIRNAMES += no-red-zone" >> "${WORK}/gcc-${GCC_VERSION}/gcc/config/i386/t-${TARGET}"

cat <<EOF >> "${WORK}/gcc-${GCC_VERSION}/gcc/config.gcc"
case \${target} in
	${TARGET})
		tmake_file="\${tmake_file} i386/t-${TARGET}"
		;;
esac
EOF
	fi

	mkdir -p "${WORK}/build/gcc"
	pushd "${WORK}/build/gcc"

	echo "Configuring GCC..."
	"${WORK}/gcc-${GCC_VERSION}/configure" \
		--quiet \
		--target="${TARGET}" \
		--prefix="${SYSROOT}" \
		--disable-nls \
		--enable-languages=c,c++ \
		--without-headers

	echo "Building GCC..."
	(make -j all-gcc && make -j all-target-libgcc && make install-gcc && make install-target-libgcc) > "${WORK}/build/gcc_build.log"
	popd
}

function build_libstdcxx() {
	mkdir -p "${WORK}/build/libstdcxx"
	pushd "${WORK}/build/libstdcxx"

	echo "Configuring libstdc++..."
	"${WORK}/gcc-${GCC_VERSION}/libstdc++-v3/configure" \
		--target="${TARGET}" \
		--prefix="${SYSROOT}" \
		--disable-nls \
		--disable-multilib \
		--with-newlib
		--disable-libstdcxx-threads

	echo "Building libstdc++..."
	(make -j && make install) > "${WORK}/build/libstdcxx_build.log"
	popd
}

function build_libc() {
	if [[ ! -d "${WORK}/poplibc" ]]; then
		echo "Downloading poplibc..."
		git clone \
			"https://github.com/justinian/poplibc.git" \
			"${WORK}/poplibc"
	else
		echo "Updating poplibc..."
		git -C "${WORK}/poplibc" pull
	fi

	pushd "${WORK}/poplibc"
	echo "Building poplibc..."
	make install PREFIX="${SYSROOT}"
	popd
}

function update_links() {
	for exe in `ls "${SYSROOT}/bin/${TARGET}-"*`; do
		base=$(echo "$exe" | sed -e "s/${TARGET}-//")
		ln -fs "${exe}" "${base}"
	done
}

build_nasm
build_binutils
build_gcc

update_links
export PATH="${SYSROOT}/bin:${PATH}"
build_libc
