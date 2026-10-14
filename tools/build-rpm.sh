#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
version=$(sed -n 's/^project(plasma-sticky-note-tabs VERSION \([^ ]*\).*/\1/p' CMakeLists.txt)
test -n "$version"
mkdir -p dist/rpmbuild/{SOURCES,SPECS,BUILD,BUILDROOT,RPMS,SRPMS}
git archive --format=tar --prefix="plasma-sticky-note-tabs-$version/" HEAD \
  | gzip -n > "dist/rpmbuild/SOURCES/plasma-sticky-note-tabs-$version.tar.gz"
cp packaging/plasma-sticky-note-tabs.spec dist/rpmbuild/SPECS/
rpmbuild -ba --define "_topdir $root/dist/rpmbuild" \
  dist/rpmbuild/SPECS/plasma-sticky-note-tabs.spec
