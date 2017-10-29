package Plugins::HueBridge::Settings;

use strict;
use warnings;

use base qw(Slim::Web::Settings);

use Data::Dumper;
use File::Spec::Functions;

use Slim::Utils::Log;
use Slim::Utils::Prefs;

use Plugins::HueBridge::HueCom;
use Plugins::HueBridge::Squeeze2Hue;
use Plugins::HueBridge::Util;

my $log   = logger('plugin.huebridge');
my $prefs = preferences('plugin.huebridge');

my $XMLConfig;
my $squeeze2hueParams;

sub name {
    return Slim::Web::HTTP::CSRF->protectName('PLUGIN_HUEBRIDGE_NAME');
}

sub page {
    return Slim::Web::HTTP::CSRF->protectURI('plugins/HueBridge/settings/basic.html');
}

sub prefs {
    return ( $prefs, qw(autosave binary binaryAutorun configFileName debugs eraselog loggingEnabled showAdvancedHueBridgeOptions numLinesLogFile) );
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
    $XMLConfig = Plugins::HueBridge::Squeeze2Hue->readXMLConfigFile();

    # Push items in XMLConfig into params for the page handlers.
    # For better recognition in the template use 'xml_' as a prefix.
    foreach my $xmlTag (keys %{$XMLConfig}) {
        
        if ( $params->{'saveSettings'} && $squeeze2hueParams->{'handler'}->{'xml_' . $xmlTag} ) {
            
            # Process the 'device':
            # First delete the macstring item.
            # Then when there was a connect request add overwrite the data with the new,
            # but only when there was a change.
            if ( $xmlTag eq 'device' ) {
            
                for (my $i = 0; $i < @{$squeeze2hueParams->{'handler'}->{'xml_' .$xmlTag}}; $i++ ) {
                    # First the cleanup.
                    delete $params->{'xml_' .$xmlTag}->[$i]->{'macstring'};
            
                    # Now the update.
                    if ( $params->{'deviceToConnect'} ) {
                        my ($deviceIndex, $deviceData) = Plugins::HueBridge::Settings::getDeviceByKey( 'udn', $params->{'deviceToConnect'}, $XMLConfig->{$xmlTag} );
                    
                        if ( $XMLConfig->{$xmlTag}->[$deviceIndex]->{$params->{'deviceToConnect'}} ne Plugins::HueBridge::HueCom->dataOfConnectedDevice() 
                             && 
                             Plugins::HueBridge::HueCom->dataOfConnectedDevice()->{'user_name'} ne 'none'
                            ) {
                            $XMLConfig->{$xmlTag}->[$deviceIndex]->{$params->{'deviceToConnect'}} = Plugins::HueBridge::HueCom->dataOfConnectedDevice();
                            $params->{'XMLConfigRestartRequested'} = 1;
                        }

                        delete $params->{'deviceToConnect'};
                    }
                }
            }
            elsif ( $xmlTag eq 'common' ) {
                # Do nothing since we do not process 'common' values.
            }
            # Push the 'xml_' prefixed params to the XMLConfig for saving,
            # but only when there was a change.
            elsif ( $XMLConfig->{$xmlTag} ne $squeeze2hueParams->{'handler'}->{'xml_' . $xmlTag} ) {
                $XMLConfig->{$xmlTag} = $squeeze2hueParams->{'handler'}->{'xml_' . $xmlTag};
                $log->debug('Tag found: ' . $squeeze2hueParams->{'handler'}->{'xml_' . $xmlTag});
                $params->{'XMLConfigRestartRequested'} = 1;
            }
        }

        $params->{'xml_' . $xmlTag} = $XMLConfig->{$xmlTag};
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
    if( $params->{'url_query'} && $params->{'url_query'} =~ m/connectHueBridge=(\w*)/g ){

            my $deviceUDN = $1;
            
            $log->debug('Triggered \'connect\' of device with udn: ' . $deviceUDN);
        
            $params->{'squeeze2hueBackgroundActionMessageBox'} = '<span id="enableConnectProgressMessageBox"></span>';
            Plugins::HueBridge::HueCom->connect( $deviceUDN, $XMLConfig );
            
            $params->{'XMLConfigRestartRequested'} = 1;
            delete $params->{'saveSettings'};
    }

    # In case we have to save settings and there is a XMLConfig change, restart.
    if ( $params->{'saveSettings'} ) {

        if ( $params->{'XMLConfigRestartRequested'} && !$params->{'squeeze2hueBackgroundActionMessageBox'} ) {

            my $XMLConfigRestartUrl = $params->{'webroot'} . $params->{'path'} . '?doXMLConfigRestart=1';
            $XMLConfigRestartUrl .= '&rand=' . $params->{'rand'} if $params->{'rand'};

            $params->{'squeeze2hueBackgroundActionMessageBox'} = '<span id="showXMLConfigRestartWarning"><a href="' . $XMLConfigRestartUrl . '"></a></span>';
            
            $params->{'XMLConfigRestartRequested'} = 0;
        }
    }

    $squeeze2hueParams->{'handler'} = $params;

    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableHueBridges {
    my ($self, $params) = @_;
    my $macString;

    $params = $squeeze2hueParams->{'handler'};
    
    if ( $params->{'xml_device'} ) {

        for( my $i = 0; $i < @{$params->{'xml_device'}}; $i++ ) {
            $macString = $params->{'xml_device'}->[$i]->{'mac'};
            $macString =~ tr/:-//d;
            $macString =~ lc $macString;

            $params->{'xml_device'}->[$i]->{'macstring'} = $macString;
        }

        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableHueBridges.html", $params);
    }
}

sub handler_file {
    my ($self, $params, undef, undef, $response) = @_;

    $params->{'url_query'} =~ m/content=(\w*)/g;
    my $fileTarget = $1;
    $params->{'url_query'} =~ m/bridge=([\w]{12})/g;
    my $macString = $1;

    $params = $squeeze2hueParams->{'handler'};
    
    delete $params->{'configfileContent'};
    delete $params->{'statefileContent'};
    delete $params->{'logfileContent'};
    
    $response->header("Refresh" => "30;");
    
    if ( $fileTarget eq 'statefile' ) {
        my $statefileName = "huebridge_" . $macString . ".state";
        my $statefile = catdir(Slim::Utils::OSDetect::dirsFor('cache'), $statefileName);
    
        $log->debug("Got request for light state file with name '" . $statefile ."'");
        $params->{'statefileContent'} = Plugins::HueBridge::Util->readFile($statefile);
    }
    elsif ( $fileTarget eq 'configfile' ) {
        $log->debug("Got Request for config file with name '" . Plugins::HueBridge::Squeeze2Hue->configFile() . "'");
        $params->{'configfileContent'} = Plugins::HueBridge::Util->readFile(Plugins::HueBridge::Squeeze2Hue->configFile());
    }
    elsif( $fileTarget eq 'logfile' ) {
        my $logContent = Plugins::HueBridge::Util->readFile( Plugins::HueBridge::Squeeze2Hue->logFile(), $prefs->get('numLinesLogFile'), 'reverse' );
        $log->debug("Got request for log file with name '" . Plugins::HueBridge::Squeeze2Hue->logFile() ."'");
        if ($logContent) {

            $params->{'logfileContent'} = $logContent;
        }
        else {

            $params->{'logfileContent'} = "Logfile empty";
        }
    }

    return Slim::Web::HTTP::filltemplatefile('plugins/HueBridge/settings/huebridge-file.html', $params);
}

sub handler_userGuide {
    my ($self, $params) = @_;
    
    $params = $squeeze2hueParams->{'handler'};
    $log->debug("Got request for userguide ('userguide.html') for display");

    return Slim::Web::HTTP::filltemplatefile('plugins/HueBridge/huebridgeguide.html', $params);
}


1;
