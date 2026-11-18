#!/bin/sh

# Analysis of the small artificial files of the Samples directory, which is in the source archive so this test
# works without the files of the regression tests and without network access (e.g. at package build time).
# The hash of the XML output, without the values which change between versions or between environments,
# is compared to the one in Samples/hashes.txt.
# GEN=1 writes the hashes of the current outputs to Samples/hashes.txt instead of comparing them (as GEN=1 of the
# FFmpeg tests), e.g. after a wanted change in the output: make check TESTS=samples.sh GEN=1

PATH_SCRIPT=$(dirname "$0")
PATH_SAMPLES=$PATH_SCRIPT/Samples
HASHES="$PATH_SAMPLES/hashes.txt"
. "$PATH_SCRIPT/utils.sh"

if ! enabled samples ; then
    exit 77 # Skip test
fi

if sha256sum --version >/dev/null 2>&1 ; then
    HASH_TOOL="sha256sum"
elif shasum -a 256 /dev/null >/dev/null 2>&1 ; then
    HASH_TOOL="shasum -a 256"
elif openssl dgst -sha256 -r /dev/null >/dev/null 2>&1 ; then
    HASH_TOOL="openssl dgst -sha256 -r"
else
    printf '%s\n' "SKIP: $(basename "$0" .sh), no SHA-256 tool (sha256sum, shasum or openssl)" >&$FD
    exit 77
fi

# XML output without the values which change between versions (version of the library) or between environments
# (path of the file, file dates, line endings)
normalized_output()
{
    analyze -f XML "$PATH_SAMPLES/$1" "$TEMP_PATH/$1.xml"
    tr -d '\r' < "$TEMP_PATH/$1.xml" \
        | sed -e 's/\(<creatingLibrary [^>]*version="\)[^"]*"/\1"/' \
              -e 's/\(<media ref="\)[^"]*"/\1"/' \
              -e '/<CompleteName>/d' \
              -e '/<FolderName>/d' \
              -e '/<File_Created_Date>/d' \
              -e '/<File_Created_Date_Local>/d' \
              -e '/<File_Modified_Date>/d' \
              -e '/<File_Modified_Date_Local>/d' \
        > "$TEMP_PATH/$1.normalized.xml"
}

output_hash()
{
    $HASH_TOOL < "$TEMP_PATH/$1.normalized.xml" | cut -d ' ' -f 1
}

if test -n "$GEN" ; then
    : > "$TEMP_PATH/hashes.txt"
    for path in "$PATH_SAMPLES"/* ; do
        i=$(basename "$path")
        case "$i" in
            generate.sh|hashes.txt) continue ;;
        esac
        normalized_output "$i"
        printf '%s  %s\n' "$(output_hash "$i")" "$i" >> "$TEMP_PATH/hashes.txt"
    done
    cp "$TEMP_PATH/hashes.txt" "$HASHES" || fail "copy of the hashes to $HASHES failed"
    printf '%s\n' "$(basename "$0" .sh): hashes written to $HASHES" >&$FD
    exit 0
fi

test -f "$HASHES" || fail "$HASHES is missing"

errors=""
count=0
while read -r expected i ; do
    test -n "$i" || continue
    count=$((count + 1))
    normalized_output "$i"
    computed=$(output_hash "$i")
    if test "$computed" != "$expected" ; then
        errors="$errors$i: hash $computed instead of $expected, output in $TEMP_PATH/$i.normalized.xml
"
    fi
done < "$HASHES"

test "$count" -gt 0 || fail "no file listed in $HASHES"
if test -n "$errors" ; then
    fail "output different from the expected one (if the change is wanted: make check TESTS=samples.sh GEN=1)" "$errors"
fi
