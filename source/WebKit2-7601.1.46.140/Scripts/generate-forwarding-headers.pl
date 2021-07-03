#!/usr/bin/perl -w
# Copyright (C) 2010 Andras Becsi (abecsi@inf.u-szeged.hu), University of Szeged
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# A script which searches for headers included by WebKit2 files
# and generates forwarding headers for these headers.

use strict;
use Cwd qw(abs_path realpath);
use File::Find;
use File::Basename;
use File::Path qw(mkpath);
use File::Spec::Functions;
use Getopt::Long;

my $srcRoot = realpath(File::Spec->catfile(dirname(abs_path($0)), "../.."));
my @platformPrefixes = ("ca", "cf", "cocoa", "Cocoa", "CoordinatedGraphics", "curl", "efl", "gtk", "ios", "mac", "soup", "win");
my @frameworks = ("JavaScriptCore", "WebCore", "WebKit");
my @skippedPrefixes;
my @frameworkHeaders;
my $framework;
my $frameworkDirectoryName;
my %neededHeaders;
my $verbose = 0; # enable it for debugging purpose

my $incFromRoot;
my $outputDirectory;
my @platform;

my %options = (
    'include-path=s' => \$incFromRoot,
    'output=s' => \$outputDirectory,
    'platform=s' => \@platform
);

GetOptions(%options);

foreach my $prefix (@platformPrefixes) {
    push(@skippedPrefixes, $prefix) if grep($_ =~ "$prefix", @platform) == 0;
}

foreach (@frameworks) {
    $framework = $_;
    $frameworkDirectoryName = ($framework eq "WebKit") ? "WebKit2" : $framework;
    @frameworkHeaders = ();
    %neededHeaders = ();

    find(\&collectNeededHeaders, abs_path($incFromRoot) );
    find(\&collectFrameworkHeaderPaths, File::Spec->catfile($srcRoot, $frameworkDirectoryName));
    createForwardingHeadersForFramework();
}

sub collectNeededHeaders {
    my $filePath = $File::Find::name;
    my $file = $_;
    if ($filePath =~ '\.h$|\.cpp$|\.c$') {
        open(FILE, "<$file") or die "Could not open $filePath.\n";
        while (<FILE>) {
           if (m/^#.*<$framework\/(.*\.h)/) {
               $neededHeaders{$1} = 1;
           }
        }
        close(FILE);
    }
}

sub collectFrameworkHeaderPaths {
    my $filePath = $File::Find::name;
    my $file = $_;
    if ($filePath =~ '\.h$' && $filePath !~ "ForwardingHeaders" && grep{$file eq $_} keys %neededHeaders) {
        my $headerPath = substr($filePath, length(File::Spec->catfile($srcRoot, $frameworkDirectoryName)) + 1 );
        push(@frameworkHeaders, $headerPath) unless (grep($headerPath =~ "$_/", @skippedPrefixes) || $headerPath =~ "config.h");
    }
}

sub createForwardingHeadersForFramework {
    my $targetDirectory = File::Spec->catfile($outputDirectory, $framework);
    mkpath($targetDirectory);
    foreach my $header (@frameworkHeaders) {
        my $headerName = basename($header);

        # If we found more headers with the same name, exit immediately.
        my @headers = grep($_ =~ "/$headerName\$", @frameworkHeaders);
        if (@headers != 1) {
            print("ERROR: Can't create $headerName forwarding header, because there are more headers with the same name:\n");
            foreach (@headers) { print " - $_\n" };
            die();
        }

        my $forwardingHeaderPath = File::Spec->catfile($targetDirectory, $headerName);
        my $expectedIncludeStatement = "#include \"$frameworkDirectoryName/$header\"";
        my $foundIncludeStatement = 0;

        $foundIncludeStatement = <EXISTING_HEADER> if open(EXISTING_HEADER, "<$forwardingHeaderPath");
        chomp($foundIncludeStatement);

        if (! $foundIncludeStatement || $foundIncludeStatement ne $expectedIncludeStatement) {
            print "[Creating forwarding header for $framework/$header]\n" if $verbose;
            open(FORWARDING_HEADER, ">$forwardingHeaderPath") or die "Could not open $forwardingHeaderPath.";
            print FORWARDING_HEADER "$expectedIncludeStatement\n";
            close(FORWARDING_HEADER);
        }

        close(EXISTING_HEADER);
    }
}
