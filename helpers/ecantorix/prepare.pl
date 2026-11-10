#!/usr/bin/env perl
# SPDX-License-Identifier: GPL-3.0-or-later
# FGA portability changes, 2026-11-10. eCantorix remains a separate GPL program.
use 5.022;
use strict;
use warnings;
use Digest::SHA qw(sha256_hex);
use File::Path qw(make_path);
use File::Copy qw(copy);
use File::Spec;

my ($source, $destination) = @ARGV;
die "Usage: perl prepare.pl UPSTREAM_DIRECTORY NEW_DIRECTORY\n" unless defined $destination;
die "Destination must not exist\n" if -e $destination;
sub read_file {
    open my $fh, '<:raw', $_[0] or die "$_[0]: $!";
    local $/; my $text = <$fh>; $text =~ s/\r\n/\n/g; return $text;
}
my $code = read_file("$source/ecantorix.pl");
die "Unsupported eCantorix source (expected commit 069317157287ce1a6390c109d1a9310556bc0002)\n"
    unless sha256_hex($code) eq 'ca9887d51cd8a9446194d8641231a6ca550e6359b02fd6241b955947df6d1d9b';
die "Unsupported Util.pm\n" unless sha256_hex(read_file("$source/eCantorix/Util.pm")) eq '3e84b5e24220858ca369c3509e709681fcb68788d78da3a50aea769a343f8834';
sub replace_once {
    my ($before, $after) = @_;
    my $at = index($code, $before);
    die "Patch context missing: $before\n" if $at < 0;
    substr($code, $at, length($before), $after);
}
my $adapter = <<'ADAPTER';
use 5.022;
# FGA_PORTABLE_GUIDE_1 — modified 2026-11-10; direct native process arguments.
# Deliberately supports the guide's default mono effects only. No shell/eval.
use File::Basename ();
sub fga_args {
    my ($mode) = @_;
    my $sox = $ENV{FGA_SOX_BIN} || 'sox';
    my @raw = ('-t', 'raw', '-r', $ENV{RATE}, '-e', 'signed', '-b', '16', '-c', '1');
    if ($mode eq 'speak') {
        return ($ENV{FGA_ESPEAK_BIN} || 'espeak', '-v', $ENV{VOICE},
            ($ENV{FGA_ESPEAK_DATA} ? ('--path='.File::Basename::dirname($ENV{FGA_ESPEAK_DATA})) : $ENV{VOICE_PATH} ? ('--path='.$ENV{VOICE_PATH}) : ()),
            '-z', '-p', $ENV{PITCH}, '-s', $ENV{SPEED}, '-a', $ENV{VELOCITY},
            '-w', $ENV{OUT}, '-m', '<prosody range="0"> '.$ENV{SYLLABLE}.' </prosody>');
    }
    die "Custom effects unsupported in FGA guide adapter\n" if ($ENV{PREEFFECTS} || $ENV{AFTEREFFECTS});
    return ($sox, $ENV{IN}, @raw, '-') if $mode eq 'read';
    return ($sox, @raw, '-', '-t', 'wav', $ENV{OUT}) if $mode eq 'write';
    return ($sox, $ENV{IN}, @raw, '-', 'remix', '-', 'silence', '1', '1s', '0', 'reverse', 'silence', '1', '1s', '0', 'reverse') if $mode eq 'trim';
    return ($sox, @raw, '-', $ENV{OUT}, 'tempo', '-s', $ENV{TEMPO},
        split(/\s+/, $ENV{PITCHBEND} || '')) if $mode eq 'stretch';
    die "Unknown guide pipe\n";
}
ADAPTER
replace_once('use eCantorix::Util;', "use eCantorix::Util;\n$adapter");
replace_once('0 == system $ESPEAK', "my \@args = fga_args('speak');\n\t\t0 == system { \$args[0] } \@args");
replace_once('open my $fh, \'-|\', "$cmd"', "open my \$fh, '-|', fga_args('read')");
replace_once('open my $fh, \'|-\', "$cmd"', "open my \$fh, '|-', fga_args('write')");
replace_once("open my \$fh, '-|', \$SOX_PROCESS_IN_TO_S16LE", "open my \$fh, '-|', fga_args('trim')");
replace_once("open my \$fh, '|-', \$SOX_PROCESS_TEMPO_PITCHBEND_S16LE_TO_OUT", "open my \$fh, '|-', fga_args('stretch')");
# Binary pipe mode is required by native Windows Perl (no CR/LF conversion).
$code =~ s/(or die "[^"\n]*: \$!";\n)(\s*\$fh;)/$1\t\tbinmode \$fh;\n$2/g;
# Also use raw mode for eCantorix's sample cache reads/writes.
$code =~ s/(open my \$fh, ")([<>])("?,?)/$1$2:raw$3/g;
make_path("$destination/eCantorix");
open my $out, '>:raw', "$destination/ecantorix.pl" or die $!;
print $out $code; close $out or die $!;
copy("$source/eCantorix/Util.pm", "$destination/eCantorix/Util.pm") or die $!;
copy("$source/COPYING", "$destination/COPYING") or die $!;
print "Prepared separate GPL guide source at $destination\n";
