package Plugins::HueBridge::Settings;

use strict;
use warnings;

use base qw(Slim::Web::Settings);

use XML::Simple;

use Data::Dumper;

use Slim::Utils::Log;
use Slim::Utils::Prefs;

use Plugins::HueBridge::HueCom;

my $log   = logger('plugin.huebridge');
my $prefs = preferences('plugin.huebridge');

sub name {
    return Slim::Web::HTTP::CSRF->protectName('PLUGIN_HUEBRIDGE_NAME');
}

sub page {
    return Slim::Web::HTTP::CSRF->protectURI('plugins/HueBridge/settings/basic.html');
}

sub prefs {
    return ( $prefs, qw(binaryAutorun showAdvancedHueBridgeOptions) );
}

sub beforeRender {
    my ($class, $params) = @_;
    
    if( Plugins::HueBridge::HueCom->getConnectProgress() || Plugins::HueBridge::HueCom->getConnectDisconnectStatus() ) {
        $params->{'statusHueBridgeConnect'} = 1;
    }
    else {
        $params->{'statusHueBridgeConnect'} = 0;
    }
}

sub handler {
    my ($class, $client, $params, $callback, @args) = @_;

    if ( $prefs->get('binaryAutorun') ) {
        $params->{'binaryRunning'} = Plugins::HueBridge::Squeeze2Hue->alive();
    }
    else {
        $params->{'binaryRunning'} = 0;
    }
    
    $params->{'helperBinary'}   = Plugins::HueBridge::Squeeze2Hue->helperBinary;
    $params->{'availableHelperBinaries'} = [ Plugins::HueBridge::Squeeze2Hue->getAvailableHelperBinaries() ];

    for( my $i = 0; defined($params->{"connectHueBridgeButtonHelper$i"}); $i++ ) {
        if( $params->{"connectHueBridge$i"} ){
            $log->debug('Triggered \'connect\' for index: ' . $i);
            Plugins::HueLightning::Hue->connect( $i );
            delete $params->{saveSettings};
        }
    }
    
    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableAdvancedHueBridgeOptions {
    my ($client, $params) = @_;
    
    if ( $prefs->get('showAdvancedHueBridgeOptions') ) {
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableAdvancedHueBridgeOptions.html", $params);
    }
}

sub handler_tableBodyHueBridges {
    my ($class, $client, $params) = @_;
    
    my $XMLConfig = readXMLConfigFile($class, KeyAttr => 'device');
    
    if ( \@{$XMLConfig->{'device'}} ) {
    #    $params->{'huebridges'} = \@{$XMLConfig->{'device'}};
    #    return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableBodyFoundHueBridges.html", $params);
    }
}

sub handler_tableHeaderHueBridges {
    my ($class, $client, $params) = @_;

    my $XMLConfig = readXMLConfigFile($class, KeyAttr => 'device');
    #$log->error(Dumper(\@{$XMLConfig->{'device'}}));

    if ( \@{$XMLConfig->{'device'}} ) {
    #    $params->{'huebridges'} = \@{$XMLConfig->{'device'}};
    #    return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableHeaderHueBridges.html", $params);
    }
}

sub readXMLConfigFile {
    my ($class,@args) = @_;
    my $ret;

    my $file = Plugins::HueBridge::Squeeze2Hue->configFile($class);
    if (-e $file) {
        $ret = XMLin($file, ForceArray => ['device'], KeepRoot => 0, NoAttr => 1, @args);
    }	

    return $ret;
}

1;
