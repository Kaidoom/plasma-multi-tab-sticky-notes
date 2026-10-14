# SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
# SPDX-License-Identifier: BSD-2-Clause
Name:           plasma-sticky-note-tabs
Version:        0.3.1
Release:        1%{?dist}
Summary:        Plasma Tabbed Notes, a KDE Plasma 6 sticky note widget with tabs
License:        GPL-2.0-or-later AND LGPL-2.1-or-later AND LGPL-2.0-or-later AND BSD-3-Clause AND BSD-2-Clause AND CC0-1.0 AND (GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL) AND (LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL)
URL:            https://github.com/Kaidoom/plasma-multi-tab-sticky-notes
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake >= 3.22
BuildRequires:  ninja-build
BuildRequires:  extra-cmake-modules >= 6.26
BuildRequires:  cmake(Qt6Core) >= 6.10
BuildRequires:  cmake(Qt6Quick) >= 6.10
BuildRequires:  cmake(KF6Config) >= 6.26
BuildRequires:  cmake(KF6CoreAddons) >= 6.26
BuildRequires:  cmake(KF6I18n) >= 6.26
BuildRequires:  cmake(KF6Svg) >= 6.26
BuildRequires:  cmake(Plasma) >= 6.7
Requires:       plasma-workspace >= 6.7
Requires:       kf6-kirigami
Requires:       kf6-kdeclarative
Requires:       kf6-kcmutils

%description
An unofficial standalone fork of KDE's Sticky Note applet, with its own plugin identity
and separate note storage. KDE's original Sticky Note remains available.

%prep
%autosetup

%build
%cmake -G Ninja -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install
%find_lang plasma_applet_dev.kaidoom.tabbednotes

%files -f plasma_applet_dev.kaidoom.tabbednotes.lang
%{_libdir}/qt6/plugins/plasma/applets/dev.kaidoom.tabbednotes.so
%{_datadir}/doc/plasma-sticky-note-tabs/
%license %{_datadir}/licenses/plasma-sticky-note-tabs/

%changelog
* Wed Oct 14 2026 Plasma Tabbed Notes contributors - 0.3.1-1
- Confirm tab closure inside the widget and cancel pending closure when hidden.

* Wed Oct 14 2026 Plasma Tabbed Notes contributors - 0.3.0-2
- Package public release documentation, attribution and preview.
