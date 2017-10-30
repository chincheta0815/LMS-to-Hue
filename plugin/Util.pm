package Plugins::HueBridge::Util;

use strict;
use warnings;
use Data::Dumper;

sub readFile {
    my ($self, $filename, $numLines, $mode) = @_;
    my @content;
    my $string;

    open(my $fileHandle, '<', $filename);
    chomp(@content = <$fileHandle>);
    close($fileHandle);

    if ( $numLines && $numLines =~ /^\d+$/ ) {
        if ( $#content <= $numLines ) {
            $numLines = $#content;
        }
        @content = @content[($#content-$numLines+1) .. $#content];
    }

    if ( $mode && $mode eq 'reverse' ) {
        @content = reverse(@content);
        $content[$#content+1] = '...';
    }

    $string = join("\n", @content);

    return $string;
}

sub getDeviceByKey {
    my ($self, $key, $value, $listpar) = @_;
    my @list = @{$listpar};
    my $i = 0;

    while (@list) {

        my $p = pop @list;
        if ($p->{$key} eq $value) {
            return ($i, $p);
        }
        $i++;
    }

    return undef;
}

1;
