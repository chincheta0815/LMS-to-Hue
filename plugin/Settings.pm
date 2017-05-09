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
    
    $params->{'helperBinary'}   = Plugins::HueBridge::Squeeze2Hue->getHelperBinary();
    $params->{'availableHelperBinaries'} = [ Plugins::HueBridge::Squeeze2Hue->getAvailableHelperBinaries() ];

    for( my $i = 0; defined($params->{"connectHueBridgeButtonHelper$i"}); $i++ ) {
        if( $params->{"connectHueBridge$i"} ){
            
            my $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
            my $device = findUDN($params->{"connectHueBridgeButtonHelper$i"}, \@{$XMLConfig->{'device'}});
            my $ip_address = $device->{'ip_address'};
            
            $log->debug('Triggered \'connect\' for index: ' . $ip_address);
            Plugins::HueBridge::HueCom->connect( $ip_address );
            
            delete $params->{saveSettings};
        }
    }

    #if ( Plugins::HueBridge::HueCom->getBridgeUserName() neq 'none' ) {
    #   $log->debug('Got user_name:' . Plugins::HueBridge::HueCom->getBridgeUserName() );
    #};

    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableAdvancedHueBridgeOptions {
    my ($client, $params) = @_;
    
    my $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
    
    if ( $XMLConfig && $prefs->get('showAdvancedHueBridgeOptions') ) {
    
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableAdvancedHueBridgeOptions.html", $params);
    }
}

sub handler_tableHueBridges {
    my ($client, $params) = @_;
    
    my $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
    
    if ( $XMLConfig->{'device'} ) {

        $params->{'huebridges'} = $XMLConfig->{'device'};
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableHueBridges.html", $params);
    }
}

sub findUDN {
    my $udn = shift(@_);
    my $listpar = shift(@_);
    my @list = @{$listpar};

    while (@list) {

        my $p = pop @list;
        if ($p->{ 'udn' } eq $udn) { return $p; }
    }

    return undef;
}

sub readXMLConfigFile {
    my (@args) = @_;
    my $ret;

    my $file = Plugins::HueBridge::Squeeze2Hue->configFile();

    if (-e $file) {
        $ret = XMLin($file, ForceArray => ['device'], KeepRoot => 0, NoAttr => 1, @args);
    }	

    return $ret;
}

1;
