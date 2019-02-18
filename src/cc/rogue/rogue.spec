Name:           rogue
Version:        5.4.4
Release:        1%{?dist}
Summary:        The original graphical adventure game

Group:          Amusements/Games
License:        BSD
URL:            http://rogue.rogueforge.net/
Source0:        http://rogue.rogueforge.net/files/rogue5.4/rogue5.4.4-src.tar.gz
Source1:        rogue.desktop
Source2:        rogue.png
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  desktop-file-utils
BuildRequires:  ncurses-devel

%description
The one, the only, the original graphical adventure game that spawned
an entire genre.

%prep
%setup -q -n %{name}%{version}


%build
%configure --enable-setgid=games --enable-scorefile=%{_var}/games/roguelike/rogue54.scr --enable-lockfile=%{_var}/games/roguelike/rogue54.lck
make %{_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT

desktop-file-install --vendor fedora                            \
        --dir ${RPM_BUILD_ROOT}%{_datadir}/applications         \
        %{SOURCE1}
mkdir -p $RPM_BUILD_ROOT/%{_datadir}/icons/hicolor/32x32/apps/
install -p -m 644 %{SOURCE2} $RPM_BUILD_ROOT/%{_datadir}/icons/hicolor/32x32/apps/


%clean
rm -rf $RPM_BUILD_ROOT

%post
touch --no-create %{_datadir}/icons/hicolor || :
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
   %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :
fi

%postun
touch --no-create %{_datadir}/icons/hicolor || :
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
   %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :
fi


%files
%defattr(-,root,root,-)
%attr(2755,games,games) %{_bindir}/rogue
%{_mandir}/man6/rogue.6.gz
%{_datadir}/applications/fedora-%{name}.desktop
%{_datadir}/icons/hicolor/32x32/apps/rogue.png
%dir %attr(0775,games,games) %{_var}/games/roguelike
%config(noreplace) %attr(0664,games,games) %{_var}/games/roguelike/rogue54.scr
%doc %{_docdir}/%{name}-%{version}


%changelog
* Sun Sep 2 2007 Wart <wart at kobold.org> 5.4.4-1
- Update to 5.4.4

* Mon Aug 20 2007 Wart <wart at kobold.org> 5.4.3-1
- Update to 5.4.3

* Sun Jul 15 2007 Wart <wart at kobold.org> 5.4.2-9
- New upstream home page and download URL
- Add patch when reading long values from the save file on 64-bit arch
  (BZ #248283)
- Add patch removing many compiler warnings
- Use proper version in the .desktop file

* Sat Mar 3 2007 Wart <wart at kobold.org> 5.4.2-8
- Use better sourceforge download url
- Use more precise desktop file categories

* Mon Aug 28 2006 Wart <wart at kobold.org> 5.4.2-7
- Rebuild for Fedora Extras

* Tue May 16 2006 Wart <wart at kobold.org> 5.4.2-6
- Added empty initial scoreboard file.

* Mon May 15 2006 Wart <wart at kobold.org> 5.4.2-5
- Better setuid/setgid handling (again) (BZ #187392)

* Thu Mar 30 2006 Wart <wart at kobold.org> 5.4.2-4
- Better setuid/setgid handling (BZ #187392)
- Resize desktop icon to match directory name

* Mon Mar 13 2006 Wart <wart at kobold.org> 5.4.2-3
- Added icon for .desktop file.

* Sun Mar 12 2006 Wart <wart at kobold.org> 5.4.2-2
- Added missing BR: ncurses-devel, desktop-file-utils

* Sat Feb 25 2006 Wart <wart at kobold.org> 5.4.2-1
- Initial spec file.
