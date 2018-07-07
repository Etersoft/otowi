#!/bin/sh

WINESOURCE=$(pwd)/wine-pure

mkdir -p include/

# from wine
for i in windows.h excpt.h windef.h winnt.h basetsd.h guiddef.h \
         "pshpack?.h" poppack.h winnt.rh winbase.h winerror.h \
         libloaderapi.h winuser.h winuser.rh winnls.h wincon.h \
         winver.h verrsrc.h winreg.h \
         reason.h winnetwk.h cderr.h dde.h dde.rh ddeml.h \
         lzexpand.h winperf.h; do
    # TODO: add comment do not change this code
    cp -f $WINESOURCE/include/$i include/
done

# modified at our side
for i in wingdi.h ; do
    rm -f include/$i
    ln -sr include.otowi/$i include/$i
done

create_stub()
{
local UP=$(basename "$1" | tr [:lower:] [:upper:] | sed -e "s|\.|_|g")
cat <<EOF > "$1"
#ifndef __WINE_$UP
#define __WINE_$UP

#endif  /* #ifdef __WINE_UP_H */
EOF
}

# just stub
for i in dlgs.h mmsystem.h nb30.h rpc.h shellapi.h \
    winsock.h wincrypt.h winscard.h winsvc.h mcx.h imm.h \
    imm.h; do
    create_stub include/$i
done
