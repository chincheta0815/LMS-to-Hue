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

my @XMLConfigPrefs = qw(device log_limit scan_interval scan_timeout user_name user_valid);

sub name {
    return Slim::Web::HTTP::CSRF->protectName('PLUGIN_HUEBRIDGE_NAME');
}

sub page {
    return Slim::Web::HTTP::CSRF->protectURI('plugins/HueBridge/settings/basic.html');
}

sub prefs {
    return ( $prefs, qw(autosave binary binaryAutorun configFileName debugs eraselog loggingEnabled showAdvancedHueBridgeOptions) );
}

sub beforeRender {
    my ($class, $params) = @_;
    
    # Before rendering the html page check if there are background actions running.
    # In case, disable html input items in the GUI.
    if( Plugins::HueBridge::HueCom->getConnectDisconnectStatus() || Plugins::HueBridge::Squeeze2Hue->getRestartStatus() ) {
    
        $params->{'statusHueBridgeBackgroundAction'} = 1;
    }
    else {
    
        $params->{'statusHueBridgeBackgroundAction'} = 0;
    }
}

sub handler {
    my ($class, $client, $params, $callback, @args) = @_;

    # Set params needed for some GUI cosmetics.
    $params->{'arch'} = Slim::Utils::OSDetect::details->{'os'};
    $params->{'availableBinaries'} = [ Plugins::HueBridge::Squeeze2Hue->getAvailableBinaries() ];
    $params->{'configFilePath'} = Slim::Utils::OSDetect::dirsFor('prefs');

    # Read in XMLConfig data from the binary config file.
    $XMLConfig = Plugins::HueBridge::Squeeze2Hue->readXMLConfigFile( KeyAttr => 'device' );

    # Handle the XMLConfig prefs.
    # Only work with those needed and listed in @XMLConfigPrefs.
    foreach my $prefName (@XMLConfigPrefs) {
        
        if ( $params->{'saveSettings'} ) {

            # Push the 'xml_' prefixed params to the XMLConfig for saving.
            $XMLConfig->{$prefName} = $params->{'xml_' . $prefName};
            
            # Request a binary restart for taking the changes into effect.
            $XMLConfigRestartRequested = 1;
        }

        # Push the XMLConfig into params for the html hanlder.
        # For better recognition in the template use 'xml_' as a prefix.
        $params->{'xml_' . $prefName} = $XMLConfig->{$prefName};
    }

    # When there is a doXMLConfigRestart sent via the web url restart and read the new xml config.
    # For showing a GUI message box to the user set a span in the html template.
    if ( $params->{'doXMLConfigRestart'} ) {
    
        Plugins::HueBridge::Squeeze2Hue->restart($XMLConfig);
        $params->{'squeeze2hueBackgroundActionMessageBox'} = '<span id="enableRestartProgressMessageBox"></span>';
        delete $params->{'saveSettings'};
    }

    if ( $params->{'generateSqueeze2HueXMLConfig'} ) {

        Plugins::HueBridge::Squeeze2Hue->restart('genXMLConfig');
        delete $params->{'saveSettings'};
    }

    if ( $params->{'deleteSqueeze2HueXMLConfig'} ) {
        
        my $conf = Plugins::HueBridge::Squeeze2Hue->configFile();
        unlink $conf;
    
        $log->debug('Deleting Squeeze2Hue XML configuration file ('. $conf .')');
    
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

    # Start the binary.
    if ( $params->{'startSqueeze2Hue'} ) {
    
        $log->debug('Trigggered \'start\' of Squeeze2Hue binary.');
        Plugins::HueBridge::Squeeze2Hue->start();
        
        delete $params->{'saveSettings'};
    }
    
    # Stop the binary.
    if ( $params->{'stopSqueeze2Hue'} ) {
    
        $log->debug('Triggered \'stop\' of Squeeze2Hue binary.');
        Plugins::HueBridge::Squeeze2Hue->stop();
        
        delete $params->{'saveSettings'};
    }
    
    # Restart the binary without any special actions.
    if ( $params->{'restartSqueeze2Hue'} ) {
    
        $log->debug('Triggered \'restart\' of Squeeze2Hue binary.');
        Plugins::HueBridge::Squeeze2Hue->restart();
        
        delete $params->{'saveSettings'};
    }

    # Get a connect trigger from the GUI and assign it with the right device.
    # For showing a GUI message box to the user set a span in the html template.
    for( my $i = 0; defined($params->{"connectHueBridgeButtonHelper$i"}); $i++ ) {
        if( $params->{"connectHueBridge$i"} ){
            
            my $deviceUDN = $params->{"connectHueBridgeButtonHelper$i"};
            
            $log->debug('Triggered \'connect\' of device with udn: ' . $deviceUDN);

            $params->{'squeeze2hueBackgroundActionMessageBox'} = '<span id="enableConnectProgressMessageBox"></span>';
            Plugins::HueBridge::HueCom->connect( $deviceUDN, $XMLConfig );
            
            $XMLConfigRestartRequested = 1;
            delete $params->{'saveSettings'};
        }
    }

    # Get a connect trigger from the GUI and assign it with the right device.
    # For showing a GUI message box to the user set a span in the html template.
    for( my $i = 0; defined($params->{"showStateFileButtonHelper$i"}); $i++ ) {
        if( $params->{"showStateFile$i"} ){

            $params->{'stateFileToShow'} = $params->{"showStateFileButtonHelper$i"};
            delete $params->{'saveSettings'};
        }
    }

    # In case we have to save settings and there is a XMLConfig change, restart.
    if ( $params->{'saveSettings'} ) {

        if ( $XMLConfigRestartRequested && ! $params->{'squeeze2hueBackgroundActionMessageBox'} ) {

            $params->{'XMLConfigRestartUrl'} = $params->{webroot} . $params->{path} . '?doXMLConfigRestart=1';
            $params->{'XMLConfigRestartUrl'} .= '&rand=' . $params->{'rand'} if $params->{'rand'};

            $params->{'squeeze2hueBackgroundActionMessageBox'} = '<span id="showXMLConfigRestartWarning"><a href="' .$params->{'XMLConfigRestartUrl'}. '"></a></span>';
            
            $XMLConfigRestartRequested = 0;
        }
    }

    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableHueBridges {
    my ($client, $params) = @_;
    my $macString;

    $XMLConfig = Plugins::HueBridge::Squeeze2Hue->readXMLConfigFile( KeyAttr => 'device' );
    
    if ( $XMLConfig->{'device'} ) {

        foreach my $prefName (@XMLConfigPrefs) {
        
            if ( $params->{'saveSettings'} ) {

                $XMLConfig->{$prefName} = $params->{'xml_' . $prefName};
                $XMLConfigRestartRequested = 1;
            }

            $macString = $XMLConfig->{'device'}->[0]->{'mac'};
            $macString =~ tr/:-//d;
            $macString =~ lc $macString;
           
            $params->{'arch'} = Slim::Utils::OSDetect::details->{'os'};
            $params->{'cacheDir'} = Slim::Utils::OSDetect::dirsFor('cache');
            $XMLConfig->{'device'}->[0]->{'cachedstatefile'} = 'huebridge_' . $macString . '.state';
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
