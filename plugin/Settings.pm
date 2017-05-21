package Plugins::HueBridge::Settings;

use strict;
use warnings;

use base qw(Slim::Web::Settings);

use XML::Simple;

use Data::Dumper;

use Slim::Utils::Log;
use Slim::Utils::Prefs;

use Plugins::HueBridge::HueCom;
use Plugins::HueBridge::Squeeze2Hue;

my $log   = logger('plugin.huebridge');
my $prefs = preferences('plugin.huebridge');

my $squeeze2HueRestartReason = restartReason('none');
my $squeeze2HueXMLConfigRestartCounter = -1;
my $squeeze2HueXMLConfigRestartTimeWait = 8;

my $XMLConfig;
my @XMLConfigSaveDeviceOptions = qw(user_name);

sub name {
    return Slim::Web::HTTP::CSRF->protectName('PLUGIN_HUEBRIDGE_NAME');
}

sub page {
    return Slim::Web::HTTP::CSRF->protectURI('plugins/HueBridge/settings/basic.html');
}

sub prefs {
    return ( $prefs, qw(binary binaryAutorun loggingEnabled showAdvancedHueBridgeOptions) );
}

sub beforeRender {
    my ($class, $params) = @_;
    
    if( Plugins::HueBridge::HueCom->getConnectProgress() || Plugins::HueBridge::HueCom->getConnectDisconnectStatus() ) {
    
        $params->{'statusHueBridgeBackgroundAction'} = 1;
    }
    else {
    
        $params->{'statusHueBridgeBackgroundAction'} = 0;
    }
    
    if ( $squeeze2HueRestartReason ) {
    
        $params->{'statusHueBridgeBackgroundAction'} = 1;
    }
    else {
    
        $params->{'statusHueBridgeBackgroundAction'} = 0;
    }
}

sub handler {
    my ($class, $client, $params, $callback, @args) = @_;

    if ( ! $XMLConfig ) {
    
        $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
    }

    if ( $params->{'deleteSqueeze2HueXMLConfig'} ) {
        
        my $conf = Plugins::HueBridge::Squeeze2Hue->configFile();
        unlink $conf;
    
        $log->debug('Deleting Squeeze2Hue XML configuration file ('. $conf .')');
    
        delete $params->{'saveSettings'};
    }

    if ( $params->{'generateSqueeze2HueXMLConfig'} ) {
    
        $log->debug('Generating Squeeze2Hue XML configuration file.');    
        $squeeze2HueRestartReason = restartReason('XMLConfigGenerate');
        
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
    
    $params->{'binary'}   = Plugins::HueBridge::Squeeze2Hue->getBinary();
    $params->{'availableBinaries'} = [ Plugins::HueBridge::Squeeze2Hue->getAvailableBinaries() ];

    for( my $i = 0; defined($params->{"connectHueBridgeButtonHelper$i"}); $i++ ) {
        if( $params->{"connectHueBridge$i"} ){
            
            my $deviceUDN = $params->{"connectHueBridgeButtonHelper$i"};
            
            $log->debug('Triggered \'connect\' of device with udn: ' . $deviceUDN);
            Plugins::HueBridge::HueCom->connect( $deviceUDN, $XMLConfig );
            
            delete $params->{'saveSettings'};
        }
    }

    if ( $params->{'saveSettings'} ) {
     
        foreach my $huebridge (@{$XMLConfig->{'device'}}) {

            foreach my $deviceOption (@XMLConfigSaveDeviceOptions) {
#                if ($params->{ $deviceOption } eq '') {
#                    delete $huebridge->{ $deviceOption };
#                }
#                else {
#                    $huebridge->{ $deviceOption } = $params->{ $deviceOption };
#                }
            $log->error('Value: ' .$huebridge->{'user_name'});
            }

        $squeeze2HueRestartReason = restartReason('XMLConfigReload');
        }
    }

    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableAdvancedHueBridgeOptions {
    my ($client, $params) = @_;
    
    if ( ! $XMLConfig ) {
    
        $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
    }
        
    if ( $XMLConfig && $prefs->get('showAdvancedHueBridgeOptions') ) {
    
        $params->{'configFilePath'} = Slim::Utils::OSDetect::dirsFor('prefs');
        $params->{'arch'} = Slim::Utils::OSDetect::OS();
    
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableAdvancedHueBridgeOptions.html", $params);
    }
}

sub handler_tableHueBridges {
    my ($client, $params) = @_;
    
    if ( ! $XMLConfig ) {
    
        $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
    }
    
    if ( $squeeze2HueRestartReason != restartReason('none') ) {
    
        $log->debug('Squeeze2Hue XMLConfig reload requested.');
        
        if ( Plugins::HueBridge::Squeeze2Hue->alive() && $squeeze2HueXMLConfigRestartCounter == -1 ) {
        
            $log->debug('Squeeze2Hue XMLConfig initiating stop of binary.');
            $squeeze2HueXMLConfigRestartCounter = 0;
            Plugins::HueBridge::Squeeze2Hue->stop();
        }
        
        if ( ($squeeze2HueXMLConfigRestartCounter >= 0) && ($squeeze2HueXMLConfigRestartCounter <= $squeeze2HueXMLConfigRestartTimeWait) ) {
        
            if ( $squeeze2HueRestartReason == restartReason('XMLConfigReload') ) {
            
                $log->debug('Squeeze2Hue XMLConfig waiting for clean writing.');
                if ( $squeeze2HueXMLConfigRestartCounter == POSIX::floor($squeeze2HueXMLConfigRestartTimeWait * .5) ) {

                        $log->debug('Squeeze2Hue XMLConfig writing data to file (' . Plugins::HueBridge::Squeeze2Hue->configFile() . ').');
                    writeXMLConfigFile($XMLConfig);
                }
            }
            elsif ( $squeeze2HueRestartReason == restartReason('XMLConfigGenerate') ) {
            
                $log->debug('Squeeze2Hue XMLConfig waiting for binary generating new config file.');
                if ( $squeeze2HueXMLConfigRestartCounter == POSIX::floor(($squeeze2HueXMLConfigRestartTimeWait * .5)) ) {

                    $log->debug('Squeeze2Hue XMLConfig initiating start of binary generating new config file.');
                    Plugins::HueBridge::Squeeze2Hue->start("-i", Plugins::HueBridge::Squeeze2Hue->configFile());
                }
            }

            $squeeze2HueXMLConfigRestartCounter++;
        }
        else {

            if ( $squeeze2HueRestartReason == restartReason('XMLConfigReload') ) {
                $log->debug('Squeeze2Hue XMLConfig initiating start of binary after config update.');
                Plugins::HueBridge::Squeeze2Hue->start();
            }
            
            $log->debug('Squeeze2Hue XMLConfig reading new XMLConfig from file (' . Plugins::HueBridge::Squeeze2Hue->configFile() . ').');
            $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
            
            $squeeze2HueXMLConfigRestartCounter = -1;
            $squeeze2HueRestartReason = restartReason('none');
        }
    }
    
    if ( $XMLConfig->{'device'} ) {

        $params->{'huebridges'} = $XMLConfig->{'device'};
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableHueBridges.html", $params);
    }
}

sub restartReason {
    my $restartReason =shift;
    
    my %restartReasons = (
        'none'  => 0,
        'XMLConfigReload' => 1,
        'XMLConfigGenerate' => 2,
    );
    
    my $returnValue = $restartReasons{$restartReason};
    
    return $returnValue;
}

sub initCLICommands {
    # Command init
    #    |requires client
    #    |  |is a query
    #    |  |  |has tags
    #    |  |  |  |function to call
    #    C  Q  T  F
    
    Slim::Control::Request::addDispatch(['hue','bridge','xmlconfig','restart','?'],
        [0, 1, 0, \&CLI_commandGetSqueeze2HueXMLConfigRestartProgress]);
}

sub CLI_commandGetSqueeze2HueXMLConfigRestartProgress {
    my $request = shift;
    
    $request->addResult('_hueBridgeXMLConfigRestartProgress', sprintf("%.2f", getSqueeze2HueXMLConfigRestartProgress()));
    
    $request->setStatusDone();
}

sub getSqueeze2HueXMLConfigRestartProgress {
    my $returnValue;

    if ( $squeeze2HueXMLConfigRestartCounter >= 0.0 ) {

        $returnValue =  $squeeze2HueXMLConfigRestartCounter / $squeeze2HueXMLConfigRestartTimeWait;
    }
    else {

        $returnValue = $squeeze2HueXMLConfigRestartCounter;
    }

    return $returnValue;
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

sub readXMLConfigFile {
    my (@args) = @_;
    my $ret;

    my $file = Plugins::HueBridge::Squeeze2Hue->configFile();

    if (-e $file) {
        $ret = XMLin($file, ForceArray => ['device'], KeepRoot => 0, NoAttr => 1, @args);
    }	

    return $ret;
}

sub writeXMLConfigFile {
    my $configData = shift;
    
    my $configFile = Plugins::HueBridge::Squeeze2Hue->configFile();
    
    XMLout($configData, RootName => "squeeze2hue", NoSort => 1, NoAttr => 1, OutputFile => $configFile);
}

1;
