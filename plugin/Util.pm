package Plugins::HueBridge::Util;

use strict;
use warnings;

sub readFile {
    my ($self, $filename) = @_;
    my $content;
    
    if ( open(my $fileHandle, '<', $filename) ) {
        read($fileHandle, $content, -s $fileHandle);
        close($fileHandle);
    }
    return $content;
}

sub getDeviceByKey {
    my ($key, $value, $listpar) = @_;
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
