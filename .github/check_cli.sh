#!/bin/sh

# CLI of MediaInfo (branch of the same name if any) with this library, and its file name test, for the modes ${1}...:
# - shared: C++ API of the installed library (same Unicode mode)
# - dll: C API of the installed library (with or without Unicode, the same results), loaded at run time, by a CLI with Unicode
# - dll_without_unicode: same, by a CLI without Unicode
# (for the dll modes, ZenLib is built again for the CLI if it has not the Unicode mode of the CLI, so after shared)
# The libraries are installed before (the CLI and the test load them from the system directories), MAKE and SUDO may be set

set -e
make="${MAKE:-make}"

if [ ! -d ../MediaInfo ] ; then
    sh .github/clone_branch.sh MediaInfo ../MediaInfo
fi

for mode in "${@}" ; do
    echo "CLI, ${mode}"
    case "${mode}" in
        shared)
            options=
            ;;
        dll|dll_without_unicode)
            unicode=yes
            if [ "${mode}" = dll_without_unicode ] ; then
                unicode=no
            fi
            options=--with-dll
            if [ "$(../ZenLib/Project/GNU/Library/libzen-config Unicode)" != "${unicode}" ] ; then
                options="--with-dll --enable-staticlibs" # ZenLib built again only as a static library
                git -C ../ZenLib clean -fdxq
                ( cd ../ZenLib/Project/GNU/Library && autoreconf -if && ./configure --enable-silent-rules --enable-unicode=${unicode} --enable-static --disable-shared && ${make} -j4 )
            fi
            ;;
        *)
            echo "unknown mode ${mode}"
            exit 1
            ;;
    esac
    ( cd ../MediaInfo/Project/GNU/CLI && autoreconf -if && ./configure --enable-silent-rules ${options} && ${make} -j4 && ${make} check )
    git -C ../MediaInfo clean -fdxq
done
