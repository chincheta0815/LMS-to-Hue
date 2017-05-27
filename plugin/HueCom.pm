package Plugins::HueBridge::HueCom;

use strict;

use Slim::Networking::SimpleAsyncHTTP;
use Slim::Utils::Log;
use Slim::Utils::Prefs;

use Data::Dumper;

use JSON::XS;

my $prefs = preferences('plugin.huebridge');
my $log = logger('plugin.huebridge');

my $connectProgressTimeWait = 29.33;
my $connectProgressCounter;
my $connectDisconnectStatus;

sub initCLICommands {
    # Command init
    #    |requires client
    #    |  |is a query
    #    |  |  |has tags
    #    |  |  |  |function to call
    #    C  Q  T  F
    
    Slim::Control::Request::addDispatch(['hue','bridge','connect','?'],
        [0, 1, 0, \&CLICommand_getHueBridgeConnectProgress]);
    
    Slim::Control::Request::addDispatch(['hue','bridge','connect','cancel'],
        [0, 1, 0, \&CLICommand_cancelHueBridgeConnect]);
}

sub CLICommand_getHueBridgeConnectProgress {
    my $request = shift;
    
    $request->addResult('_hueBridgeConnectProgress', sprintf("%.2f", Plugins::HueBridge::HueCom->getConnectProgress()));
    
    $request->setStatusDone();
}

sub CLICommand_cancelHueBridgeConnect {
    my $request = shift;
    
    if(Plugins::HueBridge::HueCom->getConnectDisconnectStatus()) {
        Plugins::HueBridge::HueCom->unconnect();
        $request->addResult('_hueBridgeConnectCancel', 1);
    }
    else {
        $request->addResult('_hueBridgeConnectCancel', 0);
    }
    
    $request->setStatusDone();
}

sub connect {
    my ($self, $deviceUDN, $XMLConfig) = @_;
    
    $connectProgressCounter = 0;
           
    $log->debug('Initiating hue bridge connect.');
    
    Slim::Utils::Timers::setTimer(undef, time() + 1, \&_sendConnectRequest,
                                                        {
                                                                deviceUDN => $deviceUDN,
                                                                XMLConfig => $XMLConfig,
                                                        },
                                                    );

    Slim::Utils::Timers::setTimer(undef, time() + $connectProgressTimeWait, \&unconnect);
    
    $connectDisconnectStatus = 1;
}

sub disconnect {
    my ($self, $deviceUDN, $XMLConfig) = @_;
    
    $connectDisconnectStatus = 1;
    
    $log->debug('Disconnecting device (' . $deviceUDN .').');
       
    $connectDisconnectStatus = 0;
}

sub unconnect {
    $log->debug('Stopping HueBridge connect.');
    
    Slim::Utils::Timers::killTimers(undef, \&_sendConnectRequest);
    Slim::Utils::Timers::killTimers(undef, \&unconnect);

    $connectDisconnectStatus = 0;
    $connectProgressCounter = 0;
}

sub getConnectProgress {
    my $returnValue;
    if ( $connectProgressCounter >= 0.0 ) {
        $returnValue =  $connectProgressCounter / $connectProgressTimeWait;
    }
    else {
        $returnValue = $connectProgressCounter;
    }

    return $returnValue;
}

sub getConnectDisconnectStatus {
    return $connectDisconnectStatus;
}

sub _sendConnectRequest {
    my ($self, $params) = @_;
    
    my $deviceUDN = $params->{'deviceUDN'};
    my $XMLConfig = $params->{'XMLConfig'};
    
    my $device = Plugins::HueBridge::Settings::getDeviceByUDN( $deviceUDN, $XMLConfig->{'device'} );
        
    my $bridgeIpAddress = $device->{'ip_address'};
    
    my $uri = join('', 'http://', $bridgeIpAddress, '/api');
    my $contentType = "application/json";
    
    my $request = { 'devicetype' => 'squeeze2hue#lms' };
    my $requestBody = encode_json($request);
    
    $connectProgressCounter++;
    
    Slim::Utils::Timers::setTimer(undef, time() + 1, \&_sendConnectRequest, {
                                                                deviceUDN => $deviceUDN,
                                                                XMLConfig => $XMLConfig,
                                                        },
                                                    );
    
    my $asyncHTTP = Slim::Networking::SimpleAsyncHTTP->new(
        \&_sendConnectRequestOK,
        \&_sendConnectReqeustERROR,
        {
            deviceUDN => $deviceUDN,
            XMLConfig => $XMLConfig,
            error       => "Can't get response.",
        },
    );

    $asyncHTTP->_createHTTPRequest(POST => ($uri, 'Content-Type' => $contentType, $requestBody));
}

sub _sendConnectRequestOK{

    my $asyncHTTP = shift;
    my $deviceUDN = $asyncHTTP->params('deviceUDN');
    my $XMLConfig = $asyncHTTP->params('XMLConfig');
    
    my $content = $asyncHTTP->content();
    
    $log->debug('Connect request sucessfully sent to hue bridge (' . $deviceUDN . ').');
    my $bridgeResponse = decode_json($asyncHTTP->content);
 
    if(exists($bridgeResponse->[0]->{error})){
        $log->info('Pairing with hue bridge (' . $deviceUDN . ') failed.');
        $log->debug('Message from hue bridge was: \'' .$bridgeResponse->[0]->{error}->{description}. '\'');
        
        my $device = Plugins::HueBridge::Settings::getDeviceByUDN( $deviceUDN, $XMLConfig->{'device'} );
        $device->{'user_name'} = 'error';
        $device->{'user_valid'} = '0';
    }
    elsif(exists($bridgeResponse->[0]->{success})) {
        $log->debug('Pairing with hue bridge (' . $deviceUDN . ' successful.');
        $log->debug('Got username \'' . $bridgeResponse->[0]->{success}->{username} . '\' from hue bridge (' . $deviceUDN . ').');
        
        my $device = Plugins::HueBridge::Settings::getDeviceByUDN( $deviceUDN, $XMLConfig->{'device'} );
        $device->{'user_name'} = $bridgeResponse->[0]->{success}->{username};
        $device->{'user_valid'} = '1';
        
        unconnect();
    }
    else{
        $log->error('Got nothing useful at all from hue bridge (' . $deviceUDN . ' ).');
    }
}

sub _sendConnectRequestERROR {
    my $asyncHTTP = shift;
    my $deviceUDN = $asyncHTTP->params('deviceUDN');
    
    $log->error('Request to hue bridge (' . $deviceUDN . ') could not be processed.');
    
    unconnect();
}

1;
