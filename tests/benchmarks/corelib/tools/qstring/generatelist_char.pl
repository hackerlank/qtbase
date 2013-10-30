#!/usr/bin/perl
# -*- mode: utf-8; tabs: nil -*-
## Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
## Copyright (C) 2014 Intel Corporation
## Contact: http://www.qt-project.org/legal
##
## This file is part of the QtCore module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia.  For licensing terms and
## conditions see http://qt.digia.com/licensing.  For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU Lesser General Public License version 2.1 requirements
## will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights.  These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3.0 as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU General Public License version 3.0 requirements will be
## met: http://www.gnu.org/copyleft/gpl.html.
##
##
## $QT_END_LICENSE$
#
# Parses a file (passed as argument) that contains a dump of pairs of
# strings and generates C source code including said data.
#
# The format of the file is:
#   LEN = <len1> <len2> <align1> <align2>\n<data1><data2>\n
# where:
#   LEN         the literal string "LEN"
#   <len1>      the length of the first data, in bytes
#   <len2>      the length of the second data, in bytes
#   <align1>    the alignment or pointer value of the first data
#   <align2>    the alignment or pointer value of the second data
#   <data1>     the first data
#   <data2>     the second data
#   \n          newline
#
# The code to write this data would be:
#   fprintf(out, "LEN = %d %d %d %d\n", len1, len2,
#          uint(quintptr(p1)) & 0xfff, uint(quintptr(p2)) & 0xfff);
#   fwrite(p1, 1, len1, out);
#   fwrite(p2, 1, len2, out);
#   fwrite("\n", 1, 1, out);

sub printCharArray($$$$) {
    $str = $_[0];
    $len = $_[1];
    $align = $_[2] & 0x3f;
    $offset = $_[3];

    $headpadding = $align & 0xf;
    $tailpadding = 16 - (($len + $headpadding) & 0xf);
    $multiplecachelines = ($align + $len) > 0x40;

    if ($multiplecachelines) {
        # if this string crosses into a new cacheline, then
        # replicate the result
        $headpadding |= ($offset & ~0x3f);
        $headpadding += 0x40
            if ($headpadding < $offset);
        $headpadding -= $offset;
        ++$cachelinecrosses;
    }

    if ($headpadding > 0) {
        print "    \"";
        for $i (1..$headpadding) {
            printf "\\%o", 256-$i;
        }
        print "\"\n";
    }

    print "        \"";
    for ($i = 0; $i < $len; $i++) {
        $c = substr($str, $i, 1);
        if (ord($c) < 0x20 || ord($c) > 0x7f || $c eq '"' || $c eq '\\') {
            printf "\\%o\"\"", ord($c);
        } else {
            print $c;
        }
    }

    if ($tailpadding > 0) {
        print "\"\n    \"";
        for $i (1..$tailpadding) {
            printf "\\%o", 256-$i;
        }
    }
    print "\" // ", $offset + $headpadding + $len + $tailpadding;
    print "+" if $multiplecachelines;
    print "\n";

    return ($offset + $headpadding, $offset + $headpadding + $len + $tailpadding);
}

print "// This is a generated file - DO NOT EDIT\n\n";

print "#include \"data.h\"\n\n";

$varname = shift @ARGV;
print "static const char ${varname}_charData[] __attribute__((aligned(64))) = {\n";
$count = 0;
$offset = 0;
$totalsize = 0;
$maxlen = 0;
$cachelinecrosses = 0;

while (1) {
    $line = readline(*STDIN);
    last unless defined($line);
    $line =~ /LEN = (\d+) (\d+) (\d+) (\d+)/ or die("Out of sync");
    $len1 = $1;
    $len2 = $2;
    $data[$count]->{len1} = $len1;
    $data[$count]->{len2} = $len2;
    $data[$count]->{align1} = $3 - 0;
    $data[$count]->{align2} = $4 - 0;

    # statistics
    $alignhistogram{$3 & 0xf}++;
    $alignhistogram{$4 & 0xf}++;
    $samealignments{$3 & 0xf}++ if ($3 & 0xf) == ($4 & 0xf);

    read STDIN, $a, $len1;
    read STDIN, $b, $len2;

    <STDIN>;                       # Eat the newline

    print "    // #$count\n" if $len1 + $len2;
    if ($len1 == 0) {
        $data[$count]->{offset1} = $offset;
    } else {
        ($data[$count]->{offset1}, $offset) =
            printCharArray($a, $len1, $data[$count]->{align1}, $offset);
        die if ($offset & 0xf) != 0;
    }
    if ($len2 == 0) {
        $data[$count]->{offset2} = $offset;
    } else {
        ($data[$count]->{offset2}, $offset) =
            printCharArray($b, $len2, $data[$count]->{align2}, $offset);
    }
    print "\n" if $len1 + $len2;
    ++$count;

    $totalsize += $len;
    $maxlen = $len if $len > $maxlen;
}
print "};\n";

print "static const StringData::Entry ${varname}_intData[] = {\n";
for $i (0..$count-1) {
    print "    {",
        $data[$i]->{len1}, ", ",
        $data[$i]->{len2}, ", ",
        $data[$i]->{offset1}, ", ",
        $data[$i]->{offset2}, ", ",
        $data[$i]->{align1}, ", ",
        $data[$i]->{align2},
        "},     // #$i\n";
    if ($data[$i]->{len1} != 0) {
        die if (($data[$i]->{offset1} & 0xf) != ($data[$i]->{align1} & 0xf));
    }
    if ($data[$i]->{len2} != 0) {
        die if (($data[$i]->{offset2} & 0xf) != ($data[$i]->{align2} & 0xf));
    }
}
print "};\n\n";

print "struct StringData $varname = {\n" .
    "    ${varname}_intData,\n" .
    "    { ${varname}_charData },\n" .
    "    $count, /* entryCount */\n" .
    "    $maxlen /* maxLength */\n" .
    "};\n\n";

printf "// average comparison length: %.4f\n", ($totalsize * 1.0 / $count);
printf "// cache-line crosses: %d (%.1f%%)\n",
    $cachelinecrosses, ($cachelinecrosses * 100.0 / $count / 2);

print "// alignment histogram:\n";
for $key (sort { $a <=> $b } keys(%alignhistogram)) {
    $value = $alignhistogram{$key};
    $samealigned = $samealignments{$key};
    printf "//   0xXXX%x = %d (%.1f%%) strings, %d (%.1f%%) of which same-aligned\n",
        $key, $value, $value * 100.0 / ($count*2),
        $samealigned, $samealigned * 100.0 / $value;
    $samealignedtotal += $samealigned;
}
printf "//   total  = %d (100%) strings, %d (%.1f%%) of which same-aligned\n",
    $count * 2, $samealignedtotal, $samealignedtotal * 100 / $count / 2;
