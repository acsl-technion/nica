(for f in  `find user-mod -name *.tcl` ; do diff -uN user/${f##user-mod/} $f ; done ) > ../scripts/mellanox-shell-scripts.patch
