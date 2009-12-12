#!/bin/env perl -w
use strict;

my $js;
open(JSFILE, "input_hinting_mode.js") or die "Failed to open file: $!";
$_ = do { local $/; <JSFILE> };
close(JSFILE);
s/\t|\r|\n/ /g;     # convert spacings to whitespaces
s/[^\/]\/\*.*?\*\///g;   # remove comments (careful: hinttags look like comment!)
s/ {2,}/ /g;        # strip whitespaces
s/(^|\(|\)|;|,|:|\}|\{|=|\+|\-|\*|\&|\||\<|\>|!) +/$1/g;
s/ +($|\(|\)|;|,|:|\}|\{|=|\+|\-|\*|\&|\||\<|\>|!)/$1/g;
s/\\/\\\\/g;        # escape backslashes
s/\"/\\\"/g;        # escape quotes
$js = $_;

open(HFILE, ">hintingmode.h") or die "Failed to open hintingmode.h: $!";
print HFILE "#define JS_SETUP ";
printf  HFILE "\"%s\"\n", $js;
close(HFILE);
exit;
