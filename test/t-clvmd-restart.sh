#!/bin/sh
# Copyright (C) 2011 Red Hat, Inc. All rights reserved.
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v.2.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

export LVM_CLVMD_BINARY="lib/clvmd"
export LVM_BINARY="lib/lvm"

. lib/test

# only clvmd based test, skip otherwise
test -e LOCAL_CLVMD || skip

aux prepare_devs 1

vgcreate --clustered y $vg $(cat DEVICES)

lvcreate -an --zero n -n $lv1 -l1 $vg
lvcreate -an --zero n -n $lv2 -l1 $vg
lvcreate -l1 $vg

lvchange -aey $vg/$lv1
lvchange -aey $vg/$lv2

"$LVM_CLVMD_BINARY" -S
sleep .2
# restarted clvmd has the same PID (no fork, only execve)
NEW_LOCAL_CLVMD=$(pgrep clvmd)
read LOCAL_CLVMD < LOCAL_CLVMD
test "$LOCAL_CLVMD" -eq "$NEW_LOCAL_CLVMD"

# FIXME: Hmm - how could we test exclusivity is preserved in singlenode ?
lvchange -an $vg/$lv1
lvchange -ay $vg/$lv1

lib/clvmd -R

vgremove -ff $vg
