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

my $squeeze2HueXMLConfigReloadRequested = 0;
my $squeeze2HueXMLConfigReloadProgress = -1;
my $squeeze2HueXMLConfigReloadTimeWait = 15;

my $XMLConfig;
my @XMLConfigSaveDeviceOptions = qw(name user_name mac codecs enabled remove_count server);

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
    
    if ( $squeeze2HueXMLConfigReloadRequested ) {
    
        $params->{'statusXMLConfigReloading'} = 1;
    }
    else {
    
        $params->{'statusXMLConfigReloading'} = 0;
    }
}

sub handler {
    my ($class, $client, $params, $callback, @args) = @_;

    $XMLConfig = readXMLConfigFile(KeyAttr => 'device');

    if ( $params->{'deleteSqueeze2HueXMLConfig'} ) {
        
        my $conf = Plugins::HueBridge::Squeeze2Hue->configFile();
        unlink $conf;
    
        $log->debug('Deleting Squeeze2Hue XML configuration file ('. $conf .')');
    
        delete $params->{'saveSettings'};
    }

    if ( $params->{'generateSqueeze2HueXMLConfig'} ) {
    
        $log->debug('Generating Squeeze2Hue XML configuration file.');    
        # put routine for generating XML config here.
        
        delete $params->{'saveSettings'};
    }
    
    if ( $params->{'cleanSqueeze2HueLogFile'} ) {

        my $logFile = Plugins::HueBridge::Squeeze2Hue->logFile();
        $log->debug('Cleaning Squeeze2Hue helper log file ' . $logFile . ')');
        
        open my $fileHandle, ">", $logFile;
        print $fileHandle;
        close $fileHandle;

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
    
    $params->{'helperBinary'}   = Plugins::HueBridge::Squeeze2Hue->getHelperBinary();
    $params->{'availableHelperBinaries'} = [ Plugins::HueBridge::Squeeze2Hue->getAvailableHelperBinaries() ];

    for( my $i = 0; defined($params->{"connectHueBridgeButtonHelper$i"}); $i++ ) {
        if( $params->{"connectHueBridge$i"} ){
            
            my $deviceUDN = $params->{"connectHueBridgeButtonHelper$i"};
            
            $log->debug('Triggered \'connect\' of device with udn: ' . $deviceUDN);
            Plugins::HueBridge::HueCom->connect( $deviceUDN, $XMLConfig );
            
            delete $params->{'saveSettings'};
        }
    }

    if ( $params->{'saveSettings'} ) {
     
        foreach my $huebridge ($XMLConfig->{'device'}) {

            for my $deviceOption (@XMLConfigSaveDeviceOptions) {
                if ($params->{ $deviceOption } eq '') {
                    delete $huebridge->{ $deviceOption };
                }
                else {
                    $huebridge->{ $deviceOption } = $params->{ $deviceOption };
                }
            }	
        }

        $squeeze2HueXMLConfigReloadRequested = 1;
    }

    return $class->SUPER::handler($client, $params, $callback, \@args);
}

sub handler_tableAdvancedHueBridgeOptions {
    my ($client, $params) = @_;
    
    if ( $XMLConfig && $prefs->get('showAdvancedHueBridgeOptions') ) {
    
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableAdvancedHueBridgeOptions.html", $params);
    }
}

sub handler_tableHueBridges {
    my ($client, $params) = @_;
    
    if ( $squeeze2HueXMLConfigReloadRequested ) {
    
        $log->debug('Squeeze2Hue XMLConfig reload requested.');
        
        if ( Plugins::HueBridge::Squeeze2Hue->alive() ) {
        
            $log->debug('Squeeze2Hue XMLConfig initiating stop of binary.');
            $squeeze2HueXMLConfigReloadProgress = 0;
            Plugins::HueBridge::Squeeze2Hue->stop();
        }
        
        if ( ($squeeze2HueXMLConfigReloadProgress >= 0) && ($squeeze2HueXMLConfigReloadProgress <= $squeeze2HueXMLConfigReloadTimeWait) ) {
        
            $log->debug('Squeeze2Hue XMLConfig waiting for clean writing.');
            if ( floor($squeeze2HueXMLConfigReloadProgress / .5) == floor($squeeze2HueXMLConfigReloadTimeWait/ .5) ) {
                $log->debug('Squeeze2Hue XMLConfig writing data to file (' . Plugins::HueBridge::Squeeze2Hue->configFile() . ').');
                writeXMLConfigFile($XMLConfig);
            }
            $squeeze2HueXMLConfigReloadProgress++;
        }
        else {
        
            $log->debug('Squeeze2Hue XMLConfig initiating start of binary.');
            Plugins::HueBridge::Squeeze2Hue->start();
            
            $log->debug('Squeeze2Hue XMLConfig reading new XMLConfig from file (' . Plugins::HueBridge::Squeeze2Hue->configFile() . ').');
            $XMLConfig = readXMLConfigFile(KeyAttr => 'device');
            
            $squeeze2HueXMLConfigReloadRequested = 0;
            $squeeze2HueXMLConfigReloadProgress = -1;
        }
    
    }
    
    if ( $XMLConfig->{'device'} ) {

        $params->{'huebridges'} = $XMLConfig->{'device'};
        return Slim::Web::HTTP::filltemplatefile("plugins/HueBridge/settings/tableHueBridges.html", $params);
    }
}

sub initCLICommands {
    # Command init
    #    |requires client
    #    |  |is a query
    #    |  |  |has tags
    #    |  |  |  |function to call
    #    C  Q  T  F
    
    Slim::Control::Request::addDispatch(['hue','bridge','xmlconfig','reload','progress'],
        [0, 1, 0, \&CLI_commandGetSqueeze2HueXMLConfigReloadProgress]);
}

sub CLI_commandGetSqueeze2HueXMLConfigReloadProgress {
    my $request = shift;
    
    $request->addResult('_hueBridgeXMLConfigReloadProgress', sprintf("%.2f", getSqueeze2HueXMLConfigReloadProgress()));
    
    $request->setStatusDone();
}

sub getSqueeze2HueXMLConfigReloadProgress {
    my $ret;
    if ( $squeeze2HueXMLConfigReloadProgress >= 0.0 ) {
        $ret =  $squeeze2HueXMLConfigReloadProgress / $squeeze2HueXMLConfigReloadTimeWait;
    }
    return $ret;
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

sub writeXMLConfigFile {
    my $configData = shift;
    
    my $configFile = Plugins::HueBridge::Squeeze2Hue->configFile();
    
    XMLout($configData, RootName => "squeeze2hue", NoSort => 1, NoAttr => 1, OutputFile => $configFile);
}

1;
