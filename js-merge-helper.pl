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

open(CFILE, "main.c") or die "Failed to open main.c: $!";
$_ = do { local $/; <CFILE> };
close(CFILE);

s/(#define JS_SETUP)/$1 "$js"/;

open(CFILE, ">tmp.c") or die "Failed to open tmp.c for writing: $!";
print CFILE $_;
close(CFILE);
