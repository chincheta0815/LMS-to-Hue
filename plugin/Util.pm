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

1;
