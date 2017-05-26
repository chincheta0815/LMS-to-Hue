package Plugins::HueBridge::Settings;

use strict;
use warnings;

use base qw(Slim::Web::Settings);

use Data::Dumper;

use Slim::Utils::Log;
use Slim::Utils::Prefs;

use Plugins::HueBridge::HueCom;
use Plugins::HueBridge::Squeeze2Hue;

my $log   = logger('plugin.huebridge');
my $prefs = preferences('plugin.huebridge');

my $XMLConfig;
my $XMLConfigRestartRequested;

my @lmsPrefs = qw(autosave binary binaryAutorun configFileName debugs eraselog loggingEnabled showAdvancedHueBridgeOptions);
my @XMLConfigPrefs = qw(device log_limit scan_interval scan_timeout user_name user_valid);

sub name {
    return Slim::Web::HTTP::CSRF->protectName('PLUGIN_HUEBRIDGE_NAME');
}

sub page {
    return Slim::Web::HTTP::CSRF->protectURI('plugins/HueBridge/settings/basic.html');
}

sub prefs {
    return ( $prefs, @lmsPrefs );
}

sub beforeRender {
    my ($class, $params) = @_;
    
    if( Plugins::HueBridge::HueCom->getConnectDisconnectStatus() || Plugins::HueBridge::Squeeze2Hue->getRestartStatus() ) {
    
        $params->{'statusHueBridgeBackgroundAction'} = 1;
    }
    else {
    
        $params->{'statusHueBridgeBackgroundAction'} = 0;
    }
}

sub handler {
    my ($class, $client, $params, $callback, @args) = @_;

    if ( $params->{'doXMLConfigRestart'} ) {
    
        Plugins::HueBridge::Squeeze2Hue->restart($XMLConfig);
        Plugins::HueBridge::HueCom->blockProgressCounter('release');
        Plugins::HueBridge::Squeeze2Hue->blockProgressCounter('release');
    }

    if ( ! $XMLConfig ) {
    
        $XMLConfig = Plugins::HueBridge::Squeeze2Hue->readXMLConfigFile(KeyAttr => 'device');
    }

    if ( $params->{'deleteSqueeze2HueXMLConfig'} ) {
        
        my $conf = Plugins::HueBridge::Squeeze2Hue->configFile();
        unlink $conf;
    
        $log->debug('Deleting Squeeze2Hue XML configuration file ('. $conf .')');
    
        delete $params->{'saveSettings'};
    }

    if ( $params->{'generateSqueeze2HueXMLConfig'} ) {
    
        $log->debug('Generating Squeeze2Hue XML configuration file.');    
        Plugins::HueBridge::Squeeze2Hue->restart('genXMLConfig');
        
        delete $params->{'saveSettings'};
    }
    
    if ( $params->{'cleanSqueeze2HueLogFile'} ) {

        my $logFile = Plugins::HueBridge::Squeeze2Hue->logFile();
        $log->debug('Cleaning Squeeze2Hue helper log file ' . $logFile . ')');
        
        open(my $fileHandle, ">", $logFile);
        print($fileHandle);
        close($fileHandle);

        delete $params->{'saveSettings'};
    }

    if ( $params->{'startSqueeze2Hue'} ) {
    
        $log->debug('Trigggered \'start\' of Squeeze2Hue binary.');
        Plugins::HueBridge::Squeeze2Hue->start();
        
        delete $params->{'saveSettings'};
    }
    
    if ( $params->{'stopSqueeze2Hue'} ) {
    
        $log->debug('Triggered \'stop\' of Squeeze2Hue binary.');
        Plugins::HueBridge::Squeeze2Hue->stop();
        
        delete $params->{'saveSettings'};
    }
    
    if ( $params->{'restartSqueeze2Hue'} ) {
    
        $log->debug('Triggered \'restart\' of Squeeze2Hue binary.');
        Plugins::HueBridge::Squeeze2Hue->restart();
        
        delete $params->{'saveSettings'};
    }

    if ( $prefs->get('binaryAutorun') ) {
    
        $params->{'binaryRunning'} = Plugins::HueBridge::Squeeze2Hue->alive();
    }
    else {
    
        $params->{'binaryRunning'} = 0;
    }

    $params->{'availableBinaries'} = [ Plugins::HueBridge::Squeeze2Hue->getAvailableBinaries() ];

    for( my $i = 0; defined($params->{"connectHueBridgeButtonHelper$i"}); $i++ ) {
        if( $params->{"connectHueBridge$i"} ){
            
            my $deviceUDN = $params->{"connectHueBridgeButtonHelper$i"};
            
            $log->debug('Triggered \'connect\' of device with udn: ' . $deviceUDN);
            Plugins::HueBridge::HueCom->connect( $deviceUDN, $XMLConfig );
            
            $XMLConfigRestartRequested = 1;
            delete $params->{'saveSettings'};
        }
    }

    if ( $params->{'saveSettings'} ) {

        if ( $XMLConfigRestartRequested ) {

            ### Okay, here we do the restart, ALWAYS. But a user warning would be awesome.
            if ( ! Plugins::HueBridge::HueCom->getConnectDisconnectStatus() || ! Plugins::HueBridge::Squeeze2Hue->getRestartStatus && ! $params->{'warning'} ) {
                $params->{'XMLConfigRestartUrl'} = $params->{webroot} . $params->{path} . '?doXMLConfigRestart=1';
                $params->{'XMLConfigRestartUrl'} .= '&rand=' . $params->{'rand'} if $params->{'rand'};

                Plugins::HueBridge::HueCom->blockProgressCounter('block');
                Plugins::HueBridge::Squeeze2Hue->blockProgressCounter('block');
                $params->{'warning'} = '<span id="popupWarning">' .Slim::Utils::Strings::string('PLUGIN_HUEBRIDGE_RESTART_BINARY_ON_XMLCONFIG_CHANGE_PROMPT', $params->{'XMLConfigRestartUrl'}). '</span>';
            }
            
            $XMLConfigRestartRequested = 0;
        }
    }

    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableAdvancedHueBridgeOptions {
    my ($client, $params) = @_;

    foreach my $prefName (@lmsPrefs) {
    
        if ( $params->{'saveSettings'} ) {
            $prefs->set($prefName, 'pref_' . $prefName);
        }
        
        $params->{'pref_' . $prefName} = $prefs->get($prefName);
    }

    if ( ! $XMLConfig ) {
    
        $XMLConfig = Plugins::HueBridge::Squeeze2Hue->readXMLConfigFile(KeyAttr => 'device');
    }
        
    if ( $XMLConfig && $prefs->get('showAdvancedHueBridgeOptions') ) {

        foreach my $prefName (@XMLConfigPrefs) {
            
            if ( $params->{'saveSettings'} ) {

                $XMLConfig->{$prefName} = $params->{'xml_' . $prefName};
                $XMLConfigRestartRequested = 1;
            }

            $params->{'xml_' . $prefName} = $XMLConfig->{$prefName};
        }
    
        $params->{'configFilePath'} = Slim::Utils::OSDetect::dirsFor('prefs');
        $params->{'arch'} = Slim::Utils::OSDetect::OS();
    
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableAdvancedHueBridgeOptions.html", $params);
    }
}

sub handler_tableHueBridges {
    my ($client, $params) = @_;
    
    if ( ! $XMLConfig ) {
    
        $XMLConfig = Plugins::HueBridge::Squeeze2Hue->readXMLConfigFile(KeyAttr => 'device');
    }
    
    if ( $XMLConfig->{'device'} ) {

        foreach my $prefName (@XMLConfigPrefs) {
        
            if ( $params->{'saveSettings'} ) {

                $XMLConfig->{$prefName} = $params->{'xml_' . $prefName};
                $XMLConfigRestartRequested = 1;
            }

            $params->{'xml_' . $prefName} = $XMLConfig->{$prefName};
        }
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableHueBridges.html", $params);
    }
}

sub getDeviceByUDN {
    my ($udn, $listpar) = @_;
    my @list = @{$listpar};

    while (@list) {

        my $p = pop @list;
        if ($p->{ 'udn' } eq $udn) { return $p; }
    }

    return undef;
}

1;
